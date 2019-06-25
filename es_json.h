#ifndef ES_JSON_H
#define ES_JSON_H

char* request_body(char* since, char* hostname, char* logkey, char* logname);
char* get_logs_from_data(char* data, char* last_es_id, char* newest_es_id);

#endif
