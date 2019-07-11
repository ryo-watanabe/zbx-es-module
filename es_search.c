#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <openssl/md5.h>
#include <time.h>

#include "es_search.h"
#include "db.h"
#include "curl_post.h"
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
        } else if (*p == '@') {
                cond->type = ITEM_LABEL;
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
        sp->message = params[4];
        zabbix_log(ES_SEARCH_LOG_LEVEL, "set_params : message=%s", sp->message);

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
        sp->label_key = NULL;
        for (i = 0; i < sp->nconds; i++) {
                set_condition(params[i + 5], &(sp->conditions[i]));
                if (sp->conditions[i].type == ITEM_LABEL) {
                        sp->label_key = sp->conditions[i].item;
                }
        }

        return sp;
}

void free_sp(struct SearchParams *sp) {
        free(sp->conditions);
        free(sp);
}

int construct_item_name(struct SearchParams *sp, char *name) {

        // making unique search item key by connecting all parameters
        char buf[256] = "";

        strcat(buf, sp->endpoint);
        strcat(buf, sp->prefix);
        strcat(buf, sp->item_key);
        strcat(buf, sp->message);

        int i;
        for (i = 0; i < sp->nconds; i++) {
                strcat(buf, sp->conditions[i].item);
                strcat(buf, sp->conditions[i].value);
        }

        MD5_CTX c;
        unsigned char md[MD5_DIGEST_LENGTH];

        MD5_Init(&c);
        MD5_Update(&c, buf, strlen(buf));
        MD5_Final(md, &c);
        for(i = 0; i < 16; i++) {
                zbx_snprintf(&name[i*2], 3, "%02x", (unsigned int)md[i]);
        }

        zabbix_log(ES_SEARCH_LOG_LEVEL, "item hash : %s", name);

        return 0;
}

int es_search(char **logs, struct SearchParams *sp, char *msg) {

        char url[256] = "";
        char name[33] = "";
        char last_es_id[33] = "";
        char newest_es_id[33] = "";
        char status[16] = "";
        char* body = NULL;
        int json_error = 0;
        bool nodata = false;

        // allocate buffer struct
        struct Buffer *buf;
        buf = (struct Buffer *)malloc(sizeof(struct Buffer));
        buf->data = NULL;
        buf->data_size = 0;

        // making key for db
        construct_item_name(sp, name);

        // get ES record id of previous search
        time_t prev_time = 0;
        time_t now = time(NULL);
        int is_new_item = get_db_item(name, last_es_id, status, &prev_time);
        time_t offset = now - prev_time;
        zabbix_log(ES_SEARCH_LOG_LEVEL, "Last status:%s Id:%s name:%s time:%ld(offset:%ldsec.)", status, last_es_id, name, prev_time, offset);
        bool prev_error = (0 == strcmp(status, "error"));
        bool offset_too_long = (offset > atoi(sp->period));

        // construct url
        strcat(url, "http://");
        strcat(url, sp->endpoint);
        strcat(url, "/");
        strcat(url, sp->prefix);
        strcat(url, "-*/_search");
        zabbix_log(ES_SEARCH_LOG_LEVEL, "URL : %s", url);

        // make request json
        body = request_body(sp);
        zabbix_log(ES_SEARCH_LOG_LEVEL, "REQUEST BODY : %s", body);

        // Do curl post
        long code = 0;
        int curl_error = curl_post(url, body, buf, msg, &code);
        if (!curl_error) {

                zabbix_log(ES_SEARCH_LOG_LEVEL, "RESPONSE CODE : %d", code);
                zabbix_log(ES_SEARCH_LOG_LEVEL, "RESPONSE BODY : %s", buf->data);

                *newest_es_id = '\0';
                if (code == 200L) {
                        // get logs in one string and newest ES record id
                        json_error = get_logs_from_data(logs, buf->data, last_es_id, newest_es_id, sp->item_key, sp->label_key, msg);
                        if (!json_error) {
                                if (*newest_es_id != '\0') {
                                        //  store newest ES record id in db
                                        zabbix_log(ES_SEARCH_LOG_LEVEL, "Newest ES id : %s", newest_es_id);
                                        zabbix_log(ES_SEARCH_LOG_LEVEL, "LOGS : %s", *logs);
                                } else {
                                        zabbix_log(ES_SEARCH_LOG_LEVEL, "No new logs");
                                        nodata = true;
                                }
                        }
                } else {
                        get_error_from_data(msg, buf->data);
                }

        }

        // store item status
        if (curl_error || json_error || code != 200L) {
                zbx_strlcpy(status, "error", 6);
        } else if (nodata) {
                zbx_strlcpy(status, "nodata", 7);
        } else {
                zbx_strlcpy(status, "ok", 3);
        }
        put_db_item(name, newest_es_id, status, is_new_item);

        // free data except log string
        free(buf->data);
        free(buf);
        free(body);
        free_sp(sp);

        if (curl_error || json_error || code != 200L) {
                return 1;
        }

        if ((prev_error || is_new_item || offset_too_long) && nodata) {
                free(*logs);
                *logs = strdup("es.log_search configured successfully (nodata)");
        }

        // If log string size is zero, return 2 then treated as 'no_data' in zabbix
        if (**logs == '\0') {
                free(*logs);
                return 2;
        }

        return 0;
}
