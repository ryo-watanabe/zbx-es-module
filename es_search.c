#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <stdbool.h>

#include "es_search.h"
#include "db.h"
#include "es_json.h"
#include "zabbix/common.h"
#include "zabbix/log.h"

#define ES_SEARCH_LOG_LEVEL LOG_LEVEL_INFORMATION

int set_search_message(char *message, struct SearchMessage *smsg) {

        if (strlen(message) > 255) {
                return 1;
        }
        // TODO: Snitaize or validate message string.

        *(smsg->buf) = '\0';
        strcat(smsg->buf, message);
        char *m = smsg->buf;

        // No search messages.
        if (*m == '*' || *m == '\0') {
                smsg->nmsg = 0;
                return 0;
        }

        // Divide messages
        m = smsg->buf;
        smsg->nmsg = 0;
        smsg->msg[smsg->nmsg++] = m;
        while (*m != '\0') {
                if (*m == '|') {
                        *m++ = '\0';
                        smsg->msg[smsg->nmsg++] = m;
                } else {
                        m++;
                }
                if (smsg->nmsg == 10) {
                        break;
                }
        }

        // debug print
        int i;
        for (i = 0; i < smsg->nmsg; i++) {
                zabbix_log(ES_SEARCH_LOG_LEVEL, "set_search_message : msg=%s", smsg->msg[i]);
        }

        return 0;
}

int set_condition(char *param, struct SearchCondition *cond) {

        char *p = param;

        // TODO: Snitaize or validate condition string.

        cond->type = ITEM_EXISTS;

        // not_exist condition, skip character '!'
        if (*p == '!') {
                cond->type = ITEM_NOT_EXIST;
                p++;
        }

        // copy condition string into item (and value) string.
        bool in_item = true;
        char *item = cond->item;
        char *value = cond->value;
        while (*p != '\0') {
                if (*p == '=') {
                        // change copy buffer to value when '=' found
                        cond->type = ITEM_IS_THE_VALUE;
                        in_item = false;
                        p++;
                } else {
                        if (in_item) {
                                *item++ = *p++;
                        } else {
                                *value++ = *p++;
                        }
                }
        }

        // close item and value string
        *item = '\0';
        *value = '\0';

        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_condition : type=%d item=%s value=%s", cond->type, cond->item, cond->value);

        return 0;
}

struct SearchParams* set_params(char **params, int nparam, char *msg) {

        if (nparam < 5) {
                strcat(msg, "Invalid number of parameters (<5)");
                return NULL;
        }

        // TODO: Must be other validation here.

        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_params : nparam=%d", nparam);

        // allocate params buffer, must be freed by caller with free_sp()
        struct SearchParams *sp = (struct SearchParams *)malloc(sizeof(struct SearchParams));

        sp->period = params[0];
        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_params : period=%s", sp->period);
        sp->endpoint = params[1];
        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_params : endpoint=%s", sp->endpoint);
        sp->prefix = params[2];
        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_params : prefix=%s", sp->prefix);
        sp->item_key = params[3];
        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_params : item_key=%s", sp->item_key);

        // parse search message string
        if (set_search_message(params[4], &(sp->smsg))) {
                strcat(msg, "Invalid search message string");
                free(sp);
                return NULL;
        }

        // parse condition strings
        sp->nconds = nparam - 5;
        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_params : nconds=%d", sp->nconds);

        // allocate conditions buffer, freed in free_sp()
        sp->conditions = (struct SearchCondition *)malloc(sp->nconds*sizeof(struct SearchCondition));
        int i;
        for (i = 0; i < sp->nconds; i++) {
                set_condition(params[i + 5], &(sp->conditions[i]));
        }

        return sp;
}

void free_sp(struct SearchParams *sp) {
        free(sp->conditions);
        free(sp);
}

// buffer struct for curl write data
struct Buffer {
        char *data;
        int data_size;
};

// callback for curl write function
size_t buffer_writer(char *ptr, size_t size, size_t nmemb, void *stream) {

        struct Buffer *buf = (struct Buffer *)stream;
        int block = size * nmemb;

        if (!buf) {
                return block;
        }

        // allocate buffer for string length (+ 1 for '\0')
        if (!buf->data) {
                buf->data = (char *)malloc(block + 1);
        } else {
                buf->data = (char *)realloc(buf->data, buf->data_size + block + 1);
        }

        // copy into buffer and close string
        if (buf->data) {
                memcpy(buf->data + buf->data_size, ptr, block);
                buf->data_size += block;
                buf->data[buf->data_size] = '\0';
        }

        return block;
}

int construct_item_name(struct SearchParams *sp, char *name) {

        // making unique search item key by connecting all parameters
        *name = '\0';

        strcat(name, sp->endpoint);
        strcat(name, sp->prefix);
        strcat(name, sp->item_key);

        int i;
        for (i = 0; i < sp->nconds; i++) {
                strcat(name, sp->conditions[i].item);
                strcat(name, sp->conditions[i].value);
        }

        return 0;
}

char* es_search(struct SearchParams *sp) {

        CURL *curl;
        struct Buffer *buf;
        struct curl_slist *headers = NULL;
        char url[256] = "";
        char name[256] = "";
        char last_es_id[32] = "";
        char newest_es_id[32] = "";
        char* body = NULL;

        // allocate buffer struct
        buf = (struct Buffer *)malloc(sizeof(struct Buffer));
        buf->data = NULL;
        buf->data_size = 0;

        // making key for db
        construct_item_name(sp, name);

        // get ES record id of previous search
        int is_new_item = get_last_es_id(name, last_es_id);
        zabbix_log(ES_SEARCH_LOG_LEVEL, "Last ES Id : %s (name:%s)", last_es_id, name);

        // construct url
        strcat(url, "http://");
        strcat(url, sp->endpoint);
        strcat(url, "/");
        strcat(url, sp->prefix);
        strcat(url, "-*/_search");
        zabbix_log(ES_SEARCH_LOG_LEVEL, "URL : %s", url);

        // set http header
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // make request json
        body = request_body(sp);
        zabbix_log(ES_SEARCH_LOG_LEVEL, "REQUEST BODY : %s", body);

        // curl setup
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buffer_writer);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(body));

        // curl execute
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        zabbix_log(ES_SEARCH_LOG_LEVEL, "RESPONSE BODY : %s", buf->data);

        // get logs in one string and newest ES record id
        *newest_es_id = '\0';
        char *ret = get_logs_from_data(buf->data, last_es_id, newest_es_id, sp->item_key);
        zabbix_log(ES_SEARCH_LOG_LEVEL, "Newest ES id : %s", newest_es_id);

        //  store newest ES record id in db
        if (*newest_es_id != '\0') {
                put_last_es_id(name, newest_es_id, is_new_item);
        }

        zabbix_log(ES_SEARCH_LOG_LEVEL, "LOGS : %s", ret);

        // free data except log string
        free(buf->data);
        free(buf);
        free(body);
        free_sp(sp);

        // If log string size is zero, return NULL then treated as 'no_data' in zabbix
        if (*ret == '\0') {
                free(ret);
                ret = NULL;
        }

        return ret;
}
