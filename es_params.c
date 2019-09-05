#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <openssl/md5.h>
#include <time.h>

#include "zabbix/common.h"
#include "zabbix/log.h"

#include "es_common.h"
#include "es_params.h"

int set_search_message(char *message, struct SearchMessage *smsg) {

        if (strlen(message) > 255) {
                return 1;
        }
        // TODO: Snitaize or validate message string.

        *(smsg->buf) = '\0';
        strcat(smsg->buf, message);
        char *m = smsg->buf;

        // No search messages.
        if (*m == '*' || *m == '\0') {
                smsg->nmsg = 0;
                return 0;
        }

        // Divide messages
        m = smsg->buf;
        smsg->nmsg = 0;
        int type_index = smsg->nmsg;
        smsg->msg[smsg->nmsg++] = m;
        smsg->type[type_index] = MESSAGE_TYPE_WORD;
        while (*m != '\0') {
                if (*m == '|') {
                        *m++ = '\0';
                        type_index = smsg->nmsg;
                        smsg->msg[smsg->nmsg++] = m;
                        smsg->type[type_index] = MESSAGE_TYPE_WORD;
                } else {
                        if (*m == ' ') {
                                smsg->type[type_index] = MESSAGE_TYPE_PHRASE;
                        }
                        m++;
                }
                if (smsg->nmsg == 10) {
                        break;
                }
        }

        // debug print
        int i;
        for (i = 0; i < smsg->nmsg; i++) {
                zabbix_log(ES_PARAMS_LOG_LEVEL, "set_search_message : msg=%s type=%d", smsg->msg[i], smsg->type[i]);
        }

        return 0;
}

int set_condition(char *param, struct SearchCondition *cond) {

        char *p = param;

        // TODO: Snitaize or validate condition string.

        cond->type = ITEM_EXISTS;

        // not_exist condition, skip character '!'
        if (*p == '!') {
                cond->type = ITEM_NOT_EXIST;
                p++;
        } else if (*p == '@') {
                cond->type = ITEM_LABEL;
                p++;
        }

        // copy condition string into item (and value) string.
        bool in_item = true;
        char *item = cond->item;
        char *value = cond->value;
        while (*p != '\0') {
                if (*p == '=') {
                        // change copy buffer to value when '=' found
                        cond->type = ITEM_IS_THE_VALUE;
                        in_item = false;
                        p++;
                } else {
                        if (in_item) {
                                *item++ = *p++;
                        } else {
                                *value++ = *p++;
                                if (*p == '*') {
                                        cond->type = ITEM_IS_THE_VALUE_WITH_WILDCARD;
                                }
                        }
                }
        }

        // close item and value string
        *item = '\0';
        *value = '\0';

        zabbix_log(ES_PARAMS_LOG_LEVEL, "set_condition : type=%d item=%s value=%s", cond->type, cond->item, cond->value);

        return 0;
}

// Construct search params from zabbix agent reauest
struct SearchParams* set_params(char **params, int nparam, char *msg, enum PARAM_TYPE type) {

        int required = 5;
        if (type == PARAM_TYPE_NUMERIC) {
                required = 4;
        }

        if (nparam < required) {
                strcat(msg, "Invalid number of parameters");
                return NULL;
        }

        // TODO: Must be other validation here.

        zabbix_log(ES_PARAMS_LOG_LEVEL, "set_params : nparam=%d", nparam);

        // allocate params buffer, must be freed by caller with free_sp()
        struct SearchParams *sp = (struct SearchParams *)malloc(sizeof(struct SearchParams));

        sp->type = type;
        sp->period = params[0];
        zabbix_log(ES_PARAMS_LOG_LEVEL, "set_params : period=%s", sp->period);
        sp->endpoint = params[1];
        zabbix_log(ES_PARAMS_LOG_LEVEL, "set_params : endpoint=%s", sp->endpoint);
        sp->prefix = params[2];
        zabbix_log(ES_PARAMS_LOG_LEVEL, "set_params : prefix=%s", sp->prefix);
        sp->item_key = params[3];
        zabbix_log(ES_PARAMS_LOG_LEVEL, "set_params : item_key=%s", sp->item_key);

        if (type == PARAM_TYPE_LOG) {
                sp->message = params[4];
                zabbix_log(ES_PARAMS_LOG_LEVEL, "set_params : message=%s", sp->message);

                // parse search message string
                if (set_search_message(sp->message, &(sp->smsg))) {
                        strcat(msg, "Invalid search message string");
                        free(sp);
                        return NULL;
                }
        }
        if (type == PARAM_TYPE_DISCOVERY) {
                sp->macro = params[4];
        }

        // parse condition strings
        sp->nconds = nparam - required;
        zabbix_log(ES_PARAMS_LOG_LEVEL, "set_params : nconds=%d", sp->nconds);

        sp->label_key = NULL;
        if (sp->nconds == 0) {
                sp->conditions = NULL;
        } else {
                // allocate conditions buffer, freed in free_sp()
                sp->conditions = (struct SearchCondition *)malloc(sp->nconds*sizeof(struct SearchCondition));
                int i;
                for (i = 0; i < sp->nconds; i++) {
                        set_condition(params[i + required], &(sp->conditions[i]));
                        if (sp->conditions[i].type == ITEM_LABEL) {
                                sp->label_key = sp->conditions[i].item;
                        }
                }
        }

        return sp;
}

struct SearchParams* set_log_search_params(char **params, int nparam, char *msg) {
        return set_params(params, nparam, msg, PARAM_TYPE_LOG);
}

struct SearchParams* set_numeric_get_params(char **params, int nparam, char *msg) {
        return set_params(params, nparam, msg, PARAM_TYPE_NUMERIC);
}

struct SearchParams* set_discovery_params(char **params, int nparam, char *msg) {
        return set_params(params, nparam, msg, PARAM_TYPE_DISCOVERY);
}

void free_sp(struct SearchParams *sp) {
        if (sp->conditions != NULL) {
                free(sp->conditions);
        }
        free(sp);
}

// Get hash name for search params
int construct_item_name(struct SearchParams *sp, char *name) {

        // making unique search item key by connecting all parameters
        char buf[256] = "";

        strcat(buf, sp->endpoint);
        strcat(buf, sp->prefix);
        strcat(buf, sp->item_key);
        if (sp->type == PARAM_TYPE_LOG) {
                strcat(buf, sp->message);
        }

        int i;
        for (i = 0; i < sp->nconds; i++) {
                strcat(buf, sp->conditions[i].item);
                strcat(buf, sp->conditions[i].value);
        }

        MD5_CTX c;
        unsigned char md[MD5_DIGEST_LENGTH];

        MD5_Init(&c);
        MD5_Update(&c, buf, strlen(buf));
        MD5_Final(md, &c);
        for(i = 0; i < 16; i++) {
                zbx_snprintf(&name[i*2], 3, "%02x", (unsigned int)md[i]);
        }

        zabbix_log(ES_PARAMS_LOG_LEVEL, "item hash : %s", name);

        return 0;
}
