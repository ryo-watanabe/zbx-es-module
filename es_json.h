#ifndef ES_JSON_H
#define ES_JSON_H

#include "es_common.h"
#include "es_params.h"

// Construct query for ES from search params
char* request_body(struct SearchParams *sp);

// Construct mutiline log result from ES search result
int get_logs_from_data(char **logs, char* data, char* last_es_id, char* newest_es_id, char* item_key, char* label_key, char *msg);

// Get double value from ES search result
int get_value_from_data(double *val, char* data, char* last_es_id, char* newest_es_id, char* item_key, char* label_key, char *msg);

// Get discovery from ES aggs result
int get_discovery_from_data(char **discovery, char* data, char* macro, char *msg);

// Get message from ES error search result
int get_error_from_data(char *msg, char* data);

#endif
