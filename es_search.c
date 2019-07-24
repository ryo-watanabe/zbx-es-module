#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <openssl/md5.h>
#include <time.h>

#include "db.h"
#include "curl_post.h"
#include "es_json.h"
#include "es_search.h"

#include "zabbix/common.h"
#include "zabbix/log.h"

// Log search through ES
int es_search(char **logs, double *val, struct SearchParams *sp, char *msg, enum SEARCH_TYPE type) {

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
        if (type == SEARCH_TYPE_LOG) {
                body = request_body(sp, 50);
        } else { //type == SEARCH_TYPE_NUMBER
                body = request_body(sp, 1);
        }
        zabbix_log(ES_SEARCH_LOG_LEVEL, "REQUEST BODY : %s", body);

        // Do curl post
        long code = 0;
        int curl_error = curl_post(url, body, buf, msg, &code);
        if (!curl_error) {

                zabbix_log(ES_SEARCH_LOG_LEVEL, "RESPONSE CODE : %d", code);
                zabbix_log(ES_SEARCH_LOG_LEVEL, "RESPONSE BODY : %s", buf->data);

                *newest_es_id = '\0';
                if (code == 200L) {

                        if (type == SEARCH_TYPE_LOG) {
                                // get logs in one string and newest ES record id
                                json_error = get_logs_from_data(logs, buf->data, last_es_id, newest_es_id, sp->item_key, sp->label_key, msg);
                        } else { //type == SEARCH_TYPE_NUMBER
                                // get number by double and newest ES record id
                                json_error = get_value_from_data(val, buf->data, last_es_id, newest_es_id, sp->item_key, sp->label_key, msg);
                        }

                        if (!json_error) {
                                if (*newest_es_id != '\0') {
                                        //  store newest ES record id in db
                                        zabbix_log(ES_SEARCH_LOG_LEVEL, "Newest ES id : %s", newest_es_id);
                                        if (type == SEARCH_TYPE_LOG) {
                                                zabbix_log(ES_SEARCH_LOG_LEVEL, "LOGS : %s", *logs);
                                        } else { //type == SEARCH_TYPE_NUMBER
                                                zabbix_log(ES_SEARCH_LOG_LEVEL, "VALUE : %.3f", *val);
                                        }
                                } else {
                                        zabbix_log(ES_SEARCH_LOG_LEVEL, "No new data");
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

        if (type == SEARCH_TYPE_LOG) {
                if ((prev_error || is_new_item || offset_too_long) && nodata) {
                        free(*logs);
                        *logs = strdup("es_search configured successfully (nodata)");
                }
                // If log string size is zero, return 2 then treated as 'no_data' in zabbix
                if (**logs == '\0') {
                        free(*logs);
                        return 2;
                }
        } else { //type == SEARCH_TYPE_NUMBER
                if (nodata) {
                        return 2;
                }
        }

        return 0;
}

// Log search through ES
int es_log(char **logs, struct SearchParams *sp, char *msg) {
        return es_search(logs, NULL, sp, msg, SEARCH_TYPE_LOG);
}

// Get uint metrics through ES
int es_uint(unsigned long *val, struct SearchParams *sp, char *msg) {
        double dval;
        int ret = es_search(NULL, &dval, sp, msg, SEARCH_TYPE_NUMBER);
        if (ret == 0) {
                *val = (unsigned long)dval;
        }
        return ret;
}

// Log double metrics through ES
int es_double(double *val, struct SearchParams *sp, char *msg) {
        return es_search(NULL, val, sp, msg, SEARCH_TYPE_NUMBER);
}
