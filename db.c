#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <time.h>

#include "db.h"
#include "zabbix/common.h"
#include "zabbix/log.h"

#define DB_FILE "/tmp/es_search.db"

int init_db() {

        sqlite3 *db;
        int rc;

	// create database file
        rc = sqlite3_open(DB_FILE, &db);
        if(rc != SQLITE_OK){
		zabbix_log(LOG_LEVEL_ERR, "SQLite open error. code=%d", rc);
                return 1;
        } else {
		zabbix_log(LOG_LEVEL_DEBUG, "SQLite database file OK.");
		// create table
	        rc = sqlite3_exec(db, "CREATE TABLE es_search (id integer primary key, name text, last_es_id text, status text, query_json text, time datetime)", NULL, NULL, NULL);
	        if(rc != SQLITE_OK){
			zabbix_log(LOG_LEVEL_ERR, "SQLite create table error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                        return 1;
	        } else {
			zabbix_log(LOG_LEVEL_DEBUG, "SQLite create table OK.");
		}
		sqlite3_close(db);
	}

        return 0;
}

int get_db_item(char* name, char* id, char* status, char** query_json, time_t* time) {

        sqlite3 *db;
        int rc;
        sqlite3_stmt *stmt;

        // set db path
        char path[64];
        *path = '\0';
        strcat(path, "/tmp/");
        strcat(path, name);
        strcat(path, ".db");

        *id = '\0';

        rc = sqlite3_open(path, &db);
        if(rc != SQLITE_OK){
                zabbix_log(LOG_LEVEL_ERR, "SQLite open error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                return 2;
        }

        // prepared statement
        sqlite3_prepare_v2(db,"SELECT last_es_id, status, query_json, strftime('%s',time) FROM es_search WHERE name=?", -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);

        // execute
        rc = sqlite3_step(stmt);
        const char *buf;
        int record_not_found = 1;
        if (rc == SQLITE_ROW) {
                buf = sqlite3_column_text(stmt, 0);
                zbx_strlcpy(id, buf, strlen(buf) + 1);

                buf = sqlite3_column_text(stmt, 1);
                zbx_strlcpy(status, buf, strlen(buf) + 1);

                buf = sqlite3_column_text(stmt, 2);
                *query_json = (char*)malloc(sizeof(char)*(strlen(buf) + 1));
                zbx_strlcpy(*query_json, buf, strlen(buf) + 1);

                *time = sqlite3_column_int64(stmt, 3);

                sqlite3_finalize(stmt);
                record_not_found = 0;
        } else {
                if (rc == 21) { // table not found
                        rc = sqlite3_exec(db, "CREATE TABLE es_search (id integer primary key, name text, last_es_id text, status text, query_json text, time datetime)", NULL, NULL, NULL);
        	        if(rc != SQLITE_OK){
        			zabbix_log(LOG_LEVEL_ERR, "SQLite create table error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                                return 2;
        	        } else {
        			zabbix_log(LOG_LEVEL_DEBUG, "SQLite create table OK.");
        		}
                } else {
                        zabbix_log(LOG_LEVEL_ERR, "SQLite select error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                }
        }

        sqlite3_close(db);

        return record_not_found;
}

int put_db_item(char* name, char* id, char* status, char* query_json, int insert) {

        sqlite3 *db;
        int rc;
        sqlite3_stmt *stmt;

        // set db path
        char path[64];
        *path = '\0';
        strcat(path, "/tmp/");
        strcat(path, name);
        strcat(path, ".db");

        rc = sqlite3_open(path, &db);
        if(rc != SQLITE_OK){
                zabbix_log(LOG_LEVEL_ERR, "SQLite open error.");
                return 2;
        }

        // prepared statement
        if (insert == 1) {
                sqlite3_prepare_v2(db,"INSERT INTO es_search (name, last_es_id, status, query_json, time) VALUES (?, ?, ?, ?, datetime('now'))", -1, &stmt, 0);
                sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, id, strlen(id), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, status, strlen(status), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 4, query_json, strlen(query_json), SQLITE_STATIC);
        } else {
                sqlite3_prepare_v2(db,"UPDATE es_search SET last_es_id=?, status=?, time=datetime('now') WHERE name=?", -1, &stmt, 0);
                sqlite3_bind_text(stmt, 1, id, strlen(id), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, status, strlen(status), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, name, strlen(name), SQLITE_STATIC);
        }

        // execute
        rc = sqlite3_step(stmt);
        int error = 0;
        if (rc != SQLITE_DONE) {
                zabbix_log(LOG_LEVEL_ERR, "SQLite insert/update error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                error = 1;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return error;
}
