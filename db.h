#ifndef DB_H
#define DB_H

int init_db();
int get_db_item(char* name, char* id, char* status, char** query_json, time_t *time);
int put_db_item(char* name, char* id, char* status, char* query_json, int insert);

#endif
