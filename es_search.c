#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>

#include "es_search.h"
#include "db.h"
#include "es_json.h"
#include "zabbix/common.h"
#include "zabbix/log.h"

struct Buffer {
        char *data;
        int data_size;
};

size_t buffer_writer(char *ptr, size_t size, size_t nmemb, void *stream) {
        struct Buffer *buf = (struct Buffer *)stream;
        int block = size * nmemb;
        if (!buf) {
                return block;
        }
        if (!buf->data) {
                buf->data = (char *)malloc(block + 1);
        } else {
                buf->data = (char *)realloc(buf->data, buf->data_size + block + 1);
        }
        if (buf->data) {
                memcpy(buf->data + buf->data_size, ptr, block);
                buf->data_size += block;
                buf->data[buf->data_size] = '\0';
        }
        return block;
}

char* es_search(char* index_prefix, char* hostname, char* logkey, char* logname) {

        CURL *curl;
        struct Buffer *buf;
        struct curl_slist *headers = NULL;
        char url[256] = "";
        char name[256] = "";
        char last_es_id[32] = "";
        char newest_es_id[32] = "";
        char* body = NULL;

        buf = (struct Buffer *)malloc(sizeof(struct Buffer));
        buf->data = NULL;
        buf->data_size = 0;

        strcat(name, index_prefix);
        strcat(name, hostname);
        strcat(name, logname);
        int is_new_item = get_last_es_id(name, last_es_id);
        zabbix_log(LOG_LEVEL_DEBUG, "Last ES Id : %s (name:%s)", last_es_id, name);

        strcat(url, "http://es.111.64.95.3.nip.io:9200/");
        strcat(url, index_prefix);
        strcat(url, "-*/_search");
        zabbix_log(LOG_LEVEL_DEBUG, "URL : %s", url);

        headers = curl_slist_append(headers, "Content-Type: application/json");

        //strcat(body, (char*)search_request(hostname, logkey, logname));
        body = request_body("now-2m", hostname, logkey, logname);
        zabbix_log(LOG_LEVEL_DEBUG, "REQUEST BODY : %s", body);

        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buffer_writer);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(body));

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        zabbix_log(LOG_LEVEL_DEBUG, "RESPONSE BODY : %s", buf->data);
        *newest_es_id = '\0';
        char *ret = get_logs_from_data(buf->data, last_es_id, newest_es_id);
        zabbix_log(LOG_LEVEL_DEBUG, "Newest ES id : %s", newest_es_id);
        if (*newest_es_id != '\0') {
                put_last_es_id(name, newest_es_id, is_new_item);
        }
        zabbix_log(LOG_LEVEL_DEBUG, "LOGS : %s", ret);
        free(buf->data);
        free(buf);
        free(body);

        if (*ret == '\0') {
                free(ret);
                ret = NULL;
        }

        return ret;
}
