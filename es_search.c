#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <sqlite3.h>
#include <stdbool.h>

#include "es_search.h"
#include "go/es_json.h"
#include "zabbix/common.h"
#include "zabbix/log.h"

#define NUM_LOG_LINES 50

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

char* request_body(char* since, char* hostname, char* logkey, char* logname) {

/*
        {
        "query":{
                "bool":{
                        "filter":[
                                {"range":{"@timestamp":{"gt":"SINCE"}}},
                                {"term":{"hostname":"HOSTNAME"}},
                                {"term":{"LOGKEY":"LOGNAME"}}
                        ]
                        }
                },
        "sort":{"@timestamp":"desc"},
        "size":3
        }
*/

        json_t *root = json_object();

        json_t *query = json_object();
        json_object_set_new( root, "query", query );

        json_t *obj_bool = json_object();
        json_object_set_new( query, "bool", obj_bool );

        json_t *filter_array = json_array();
        json_object_set_new( obj_bool, "filter", filter_array );

        json_t *filter0 = json_object();
        json_t *range = json_object();
        json_t *timestamp = json_object();
        json_array_append( filter_array, filter0 );
        json_object_set_new( filter0, "range", range );
        json_object_set_new( range, "@timestamp", timestamp );
        json_object_set_new( timestamp, "gt", json_string(since) );

        json_t *filter1 = json_object();
        json_t *term1 = json_object();
        json_array_append( filter_array, filter1 );
        json_object_set_new( filter1, "term", term1 );
        json_object_set_new( term1, "hostname", json_string(hostname) );

        json_t *filter2 = json_object();
        json_t *term2 = json_object();
        json_array_append( filter_array, filter2 );
        json_object_set_new( filter2, "term", term2 );
        json_object_set_new( term2, logkey, json_string(logname) );

        json_t *sort = json_object();
        json_object_set_new( root, "sort", sort );
        json_object_set_new( sort, "@timestamp", json_string("desc") );

        json_object_set_new( root, "size", json_integer(NUM_LOG_LINES) );

        char* body = NULL;
        body = json_dumps(root, 0);

        json_decref(root);

        return body;
}

char* get_logs_from_data(char* data, char* last_es_id, char* newest_es_id) {

        char* logs = NULL;

        json_error_t jerror;
        json_t *jdata = json_loads(data, 0, &jerror);

        if (jdata == NULL) {
                logs = (char*)malloc(sizeof(char)*(strlen(jerror.text) + 1));
                zbx_strlcpy(logs, jerror.text, strlen(jerror.text));
                return logs;
        }

/*
        {
        "took":30,
        "timed_out":false,
        "_shards":{
                "total":300,
                "successful":300,
                "skipped":295,
                "failed":0
                },
        "hits":{
                "total":26,
                "max_score":null,
                "hits":[{
                        "_index":"kubernetes_cluster-2019.06.19",
                        "_type":"flb_type",
                        "_id":"r_6PbWsBS9x271TQR2Xt",
                        "_score":null,
                        "_source":{
                                "@timestamp":"2019-06-19T02:27:14.988Z",
                                "_flb-key":"syslog.syslog",
                                "log":"Jun 19 11:27:14 cluster02w3 kernel: [19778089.329875] device veth3c2fa68 left promiscuous mode",
                                "hostname":"cluster02w3",
                                "log_name":"/var/log/syslog"
                                },
                        "sort":[1560911234988]
                        },
                        :
                        ]
                }
        }
*/

        json_t *hits = json_object_get(jdata, "hits");
        int total = json_integer_value(json_object_get(hits, "total"));
        json_t *hitarr = json_object_get(hits, "hits");
        int num_logs = json_array_size(hitarr);

        const char *logdata[NUM_LOG_LINES + 1];
        json_t *hit, *source;
        char *es_id;
        int i, length = 0;
        bool overwrapped = false;
        for (i = 0; i < num_logs; i++) {
                hit = json_array_get(hitarr, i);
                source = json_object_get(hit, "_source");
                es_id = (char*)json_string_value(json_object_get(hit, "_id"));
                if (0 == strcmp(last_es_id, es_id)) {
                        num_logs = i;
                        overwrapped = true;
                        break;
                }
                logdata[i] = json_string_value(json_object_get(source, "log"));
                length += strlen(logdata[i]);
                if (i == 0) {
                        zbx_strlcpy(newest_es_id, es_id, strlen(es_id) + 1);
                }
        }
        if (!overwrapped && total > num_logs) {
                char msg[] = "and more...";
                logdata[num_logs] = msg;
                length += strlen(logdata[num_logs]);
                num_logs++;
        }
        int lfs = 0;
        if (num_logs > 1) {
                lfs = num_logs - 1;
        }
        logs = (char*)malloc(sizeof(char)*(length + lfs + 1));
        *logs = '\0';
        for (i = 0; i < num_logs; i++) {
                strcat(logs, logdata[i]);
                if (i < num_logs - 1 && logs[strlen(logs) - 1] != '\n') {
                        strcat(logs, "\n");
                }
        }
        json_decref(jdata);
        return logs;
}

int get_last_es_id(char* name, char* id) {

        sqlite3 *db;
        char *filename="/tmp/es_search.db";
        int rc;
        sqlite3_stmt *stmt;

        *id = '\0';

        rc = sqlite3_open(filename, &db);
        if(rc != SQLITE_OK){
                zabbix_log(LOG_LEVEL_INFORMATION, "SQLite open error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                return 2;
        }

        // prepared statement
        sqlite3_prepare_v2(db,"SELECT last_es_id FROM es_search WHERE name=?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);

        // execute
        rc = sqlite3_step(stmt);
        const char *buf;
        int record_not_found = 1;
        if (rc == SQLITE_ROW) {
                buf = sqlite3_column_text(stmt, 0);
                zbx_strlcpy(id, buf, strlen(buf) + 1);
                record_not_found = 0;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return record_not_found;
}

int put_last_es_id(char* name, char* id, int insert) {

        sqlite3 *db;
        char *filename="/tmp/es_search.db";
        int rc;
        sqlite3_stmt *stmt;

        rc = sqlite3_open(filename, &db);
        if(rc != SQLITE_OK){
                zabbix_log(LOG_LEVEL_INFORMATION, "SQLite open error.");
                return 2;
        }

        // prepared statement
        if (insert == 1) {
                sqlite3_prepare_v2(db,"INSERT INTO es_search (name, last_es_id) VALUES (?, ?)", -1, &stmt, 0);
                sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, id, strlen(id), SQLITE_STATIC);
        } else {
                sqlite3_prepare_v2(db,"UPDATE es_search SET last_es_id=? WHERE name=?", -1, &stmt, 0);
                sqlite3_bind_text(stmt, 1, id, strlen(id), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, name, strlen(name), SQLITE_STATIC);
        }

        // execute
        rc = sqlite3_step(stmt);
        int error = 0;
        if (rc != SQLITE_DONE) {
                zabbix_log(LOG_LEVEL_INFORMATION, "SQLite insert/update error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                error = 1;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return error;
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
        zabbix_log(LOG_LEVEL_INFORMATION, "Last ES Id : %s (name:%s)", last_es_id, name);

        strcat(url, "http://es.111.64.95.3.nip.io:9200/");
        strcat(url, index_prefix);
        strcat(url, "-*/_search");
        zabbix_log(LOG_LEVEL_INFORMATION, "URL : %s", url);

        headers = curl_slist_append(headers, "Content-Type: application/json");

        //strcat(body, (char*)search_request(hostname, logkey, logname));
        body = request_body("now-2m", hostname, logkey, logname);
        zabbix_log(LOG_LEVEL_INFORMATION, "REQUEST BODY : %s", body);

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

        zabbix_log(LOG_LEVEL_INFORMATION, "RESPONSE BODY : %s", buf->data);
        *newest_es_id = '\0';
        char *ret = get_logs_from_data(buf->data, last_es_id, newest_es_id);
        zabbix_log(LOG_LEVEL_INFORMATION, "Newest ES id : %s", newest_es_id);
        if (*newest_es_id != '\0') {
                put_last_es_id(name, newest_es_id, is_new_item);
        }
        zabbix_log(LOG_LEVEL_INFORMATION, "LOGS : %s", ret);
        free(buf->data);
        free(buf);
        free(body);

        if (*ret == '\0') {
                free(ret);
                ret = NULL;
        }

        return ret;
}
