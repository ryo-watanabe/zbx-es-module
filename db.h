#ifndef DB_H
#define DB_H

int init_db();
int get_last_es_id(char* name, char* id);
int put_last_es_id(char* name, char* id, int insert);

#endif
