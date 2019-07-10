#ifndef ES_JSON_H
#define ES_JSON_H

char* request_body(struct SearchParams *sp);
int get_logs_from_data(char **logs, char* data, char* last_es_id, char* newest_es_id, char* item_key, char* label_key, char *msg);
int get_error_from_data(char *msg, char* data);

#endif
