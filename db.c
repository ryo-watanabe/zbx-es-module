#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <sqlite3.h>
//#include <stdbool.h>

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
	        rc = sqlite3_exec(db, "CREATE TABLE es_search (id integer primary key, name text, last_es_id text, status text)", NULL, NULL, NULL);
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

int get_db_item(char* name, char* id, char* status) {

        sqlite3 *db;
        int rc;
        sqlite3_stmt *stmt;

        *id = '\0';

        rc = sqlite3_open(DB_FILE, &db);
        if(rc != SQLITE_OK){
                zabbix_log(LOG_LEVEL_ERR, "SQLite open error. code=%d. msg=%s", rc, sqlite3_errmsg(db));
                return 2;
        }

        // prepared statement
        sqlite3_prepare_v2(db,"SELECT last_es_id, status FROM es_search WHERE name=?", -1, &stmt, 0);
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
                record_not_found = 0;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);

        return record_not_found;
}

int put_db_item(char* name, char* id, char* status, int insert) {

        sqlite3 *db;
        int rc;
        sqlite3_stmt *stmt;

        rc = sqlite3_open(DB_FILE, &db);
        if(rc != SQLITE_OK){
                zabbix_log(LOG_LEVEL_ERR, "SQLite open error.");
                return 2;
        }

        // prepared statement
        if (insert == 1) {
                sqlite3_prepare_v2(db,"INSERT INTO es_search (name, last_es_id, status) VALUES (?, ?, ?)", -1, &stmt, 0);
                sqlite3_bind_text(stmt, 1, name, strlen(name), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, id, strlen(id), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, status, strlen(status), SQLITE_STATIC);
        } else {
                sqlite3_prepare_v2(db,"UPDATE es_search SET last_es_id=?, status=? WHERE name=?", -1, &stmt, 0);
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
