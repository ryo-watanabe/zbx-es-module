#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include <stdbool.h>

#include "zabbix/common.h"
#include "zabbix/log.h"
#include "es_json.h"

#define NUM_LOG_LINES 50

// Construct query for ES
char* request_body(struct SearchParams *sp) {

/*
        {
        "query":{
                "bool":{
                        "filter":[
                                {"range":{"@timestamp":{"gt":"now-2m"}}},
                                {"term":{"hostname":"cluster02m1"}},
                                {"term":{"container_name":"kubelet"}}
                        ],
                        "must":[
                                {"exists":{"field":"container_name"}}
                        ],
                        "must_not":[
                                {"exists":{"field":"ignore_alerts"}}
                        ],
                        "should":[
                                {"term":{"log":"error"}},
                                {"term":{"log":"warning"}},
                                {"term":{"log":"failed"}}
                        ],
                        "minimum_should_match": 1
                        }
                },
        "sort":{"@timestamp":"desc"},
        "size":3
        }
*/

        // root
        json_t *root = json_object();

        // root > querry
        json_t *query = json_object();
        json_object_set_new( root, "query", query );

        // querry > bool
        json_t *obj_bool = json_object();
        json_object_set_new( query, "bool", obj_bool );

        // bool > filter
        json_t *filter_array = json_array();
        json_object_set_new( obj_bool, "filter", filter_array );

        // bool > must
        json_t *must_array = json_array();
        json_object_set_new( obj_bool, "must", must_array );

        // bool > must_not
        json_t *must_not_array = json_array();
        json_object_set_new( obj_bool, "must_not", must_not_array );

        // bool > should
        json_t *should_array = json_array();
        json_object_set_new( obj_bool, "should", should_array );

        // filter > range
        json_t *filter0 = json_object();
        json_t *range = json_object();
        json_t *timestamp = json_object();
        json_array_append( filter_array, filter0 );
        json_object_set_new( filter0, "range", range );
        json_object_set_new( range, "@timestamp", timestamp );
        char since[16] = "now-";
        strcat(since, sp->period);
        strcat(since, "s");
        json_object_set_new( timestamp, "gt", json_string(since) );

        // Conditions.
        //json_t **filterN = (json_t **)malloc(sp->nconds*sizeof(json_t));
        //json_t **termN = (json_t **)malloc(sp->nconds*sizeof(json_t));
        json_t *filterN[10];
        json_t *termN[10];
        int i;
        for (i = 0; i < sp->nconds; i++) {

                if (i >= 10) {
                        break;
                }

                filterN[i] = json_object();
                termN[i] = json_object();

                // filter > term
                if (sp->conditions[i].type == ITEM_IS_THE_VALUE) {
                        json_array_append( filter_array, filterN[i] );
                        json_object_set_new( filterN[i], "term", termN[i] );
                        json_object_set_new( termN[i], sp->conditions[i].item, json_string(sp->conditions[i].value) );
                }

                // must > exists
                if (sp->conditions[i].type == ITEM_EXISTS || sp->conditions[i].type == ITEM_LABEL) {
                        json_array_append( must_array, filterN[i] );
                        json_object_set_new( filterN[i], "exists", termN[i] );
                        json_object_set_new( termN[i], "field", json_string(sp->conditions[i].item) );
                }

                // must_not > exists
                if (sp->conditions[i].type == ITEM_NOT_EXIST) {
                        json_array_append( must_not_array, filterN[i] );
                        json_object_set_new( filterN[i], "exists", termN[i] );
                        json_object_set_new( termN[i], "field", json_string(sp->conditions[i].item) );
                }
        }

        // Search Messages.
        //json_t **msgFilterN;
        //json_t **msgTermN;
        json_t *msgFilterN[10];
        json_t *msgTermN[10];

        if (sp->smsg.nmsg > 0) {

                //msgFilterN = (json_t **)malloc(sp->smsg.nmsg*sizeof(json_t));
                //msgTermN = (json_t **)malloc(sp->smsg.nmsg*sizeof(json_t));
                for (i = 0; i < sp->smsg.nmsg; i++) {

                        if (i >= 10) {
                                break;
                        }

                        // should > term
                        msgFilterN[i] = json_object();
                        msgTermN[i] = json_object();
                        json_array_append( should_array, msgFilterN[i] );
                        json_object_set_new( msgFilterN[i], "term", msgTermN[i] );
                        json_object_set_new( msgTermN[i], sp->item_key, json_string(sp->smsg.msg[i]) );

                }
                // bool > minimum_should_match
                json_object_set_new( obj_bool, "minimum_should_match", json_integer(1) );
        }

        // root > sort
        json_t *sort = json_object();
        json_object_set_new( root, "sort", sort );
        json_object_set_new( sort, "@timestamp", json_string("desc") );

        // root > size
        json_object_set_new( root, "size", json_integer(NUM_LOG_LINES) );

        // Json string allocated and copied here, must be freed by caller.
        char* body = NULL;
        body = json_dumps(root, 0);

        // Free all materials to make.
        json_decref(root);
        //free(filterN);
        //free(termN);
        //if (sp->smsg.nmsg > 0) {
        //        free(msgFilterN);
        //        free(msgTermN);
        //}

        return body;
}

// json_object_get through hierarchy (max 5 levels)
json_t* json_hierarchy_object_get(json_t *data, char* key) {
        char *keycopy = strdup(key);

        // Divide key
        char *keys[5];
        char *k = keycopy;
        int h = 0;
        keys[h++] = k;
        while (*k != '\0') {
                if (*k == '.') {
                        *k++ = '\0';
                        keys[h++] = k;
                } else {
                        k++;
                }
                if (h == 5) {
                        break;
                }
        }

        // Down along hierarchy
        int i;
        for (i = 0; i < h; i++) {
                data = json_object_get(data, keys[i]);
        }

        free(keycopy);
        return data;
}

// Construct mutiline log result from ES search result
int get_logs_from_data(char **logs, char* data, char* last_es_id, char* newest_es_id, char* item_key, char* label_key, char *msg) {

        json_error_t jerror;
        json_t *jdata = json_loads(data, 0, &jerror);

        if (jdata == NULL) {
                zbx_strlcpy(msg, jerror.text, MESSAGE_MAX);
                return 1;
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
        // hits
        json_t *hits = json_object_get(jdata, "hits");

        // hits > total
        int total = json_integer_value(json_object_get(hits, "total"));

        // hits > hists[]
        json_t *hitarr = json_object_get(hits, "hits");
        int num_logs = json_array_size(hitarr);

        // loop through hits[] and get hists[] > _source > log
        const char *logdata[NUM_LOG_LINES + 1];
        const char *labels[NUM_LOG_LINES + 1];
        json_t *hit, *source;
        char *es_id;
        int i, length = 0;
        bool overwrapped = false;
        for (i = 0; i < num_logs; i++) {

                hit = json_array_get(hitarr, i);

                // hits[] > _source
                source = json_object_get(hit, "_source");

                // hits[] > _id, break when previous id found.
                es_id = (char*)json_string_value(json_object_get(hit, "_id"));
                if (0 == strcmp(last_es_id, es_id)) {
                        num_logs = i;
                        overwrapped = true;
                        break;
                }

                // _source > log, get log string pointer and calculate length.
                logdata[i] = json_string_value(json_object_get(source, item_key));
                length += strlen(logdata[i]);

                if (label_key != NULL) {
                        labels[i] = json_string_value(json_hierarchy_object_get(source, label_key));
                        length += strlen(labels[i]) + 3;
                }

                // preserve newest id for next search
                if (i == 0) {
                        zbx_strlcpy(newest_es_id, es_id, strlen(es_id) + 1);
                }

        }

        // buffer not enough (not overwrapped with previous search), add comment 'and more...'
        if (!overwrapped && total > num_logs) {
                char msg[] = "and more...";
                logdata[num_logs] = msg;
                char empty[] = "";
                labels[num_logs] = empty;
                length += strlen(logdata[num_logs]);
                num_logs++;
        }

        // calculate number of LF
        int lfs = 0;
        if (num_logs > 1) {
                lfs = num_logs - 1;
        }

        // allocate result log string buffer
        char *buf = (char*)malloc(sizeof(char)*(length + lfs + 1));

        // copy log lines
        *buf = '\0';
        for (i = 0; i < num_logs; i++) {

                if (label_key != NULL && *(labels[i]) != '\0') {
                        strcat(buf, "[");
                        strcat(buf, labels[i]);
                        strcat(buf, "] ");
                }
                strcat(buf, logdata[i]);

                // insert LF when log line not end with LF
                if (i < num_logs - 1 && buf[strlen(buf) - 1] != '\n') {
                        strcat(buf, "\n");
                }
        }

        // free json data
        json_decref(jdata);

        *logs = buf;

        return 0;
}

// Get message from ES error search result
int get_error_from_data(char *msg, char* data) {

        json_error_t jerror;
        json_t *jdata = json_loads(data, 0, &jerror);

        if (jdata == NULL) {
                zbx_strlcpy(msg, jerror.text, MESSAGE_MAX);
                return 1;
        }

/*
        {
        "error":{
                "root_cause":[{
                        "type":"illegal_argument_exception",
                        "reason":"field name is null or empty"
                        }],
                "type":"illegal_argument_exception",
                "reason":"field name is null or empty"
                },
        "status":400
        }
*/
        // error
        json_t *error = json_object_get(jdata, "error");

        // error > root_cause[0]
        json_t *root_cause_array = json_object_get(error, "root_cause");
        json_t *root_cause0 = json_array_get(root_cause_array, 0);

        // copy error type and reason
        strcat(msg, json_string_value(json_object_get(root_cause0, "type")));
        strcat(msg, " : ");
        strcat(msg, json_string_value(json_object_get(root_cause0, "reason")));

        // free json data
        json_decref(jdata);

        return 0;
}
