#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <stdbool.h>

#include "zabbix/common.h"
#include "zabbix/log.h"

#define NUM_LOG_LINES 50

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
