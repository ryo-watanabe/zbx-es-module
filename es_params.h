#ifndef ES_PARAMS_H
#define ES_PARAMS_H

// Parameter types
enum PARAM_TYPE {
        PARAM_TYPE_LOG,
        PARAM_TYPE_NUMERIC,
        PARAM_TYPE_DISCOVERY
};

// Condition types for query
enum CONDITION_TYPE {
        ITEM_IS_THE_VALUE,
        ITEM_IS_THE_VALUE_WITH_WILDCARD,
        ITEM_EXISTS,
        ITEM_NOT_EXIST,
        ITEM_LABEL
};

// Message types for log filtering
enum MESSAGE_TYPE {
        MESSAGE_TYPE_WORD,
        MESSAGE_TYPE_PHRASE
};

// Struct for constructing query
struct SearchCondition {
        enum CONDITION_TYPE type;
        char item[32];
        char value[32];
};

// Messages for log filtering
// buf = "msgA|msgB|msgC|..."
// msg[0] = "msgA", mgs[1] = "msgB", ...
struct SearchMessage {
        int nmsg;
        enum MESSAGE_TYPE type[10];
        char *msg[10];
        char buf[256];
};

// Parameter holder for ES search query
struct SearchParams {
        char *period;
        char *endpoint;
        char *prefix;
        char *item_key;
        char *macro;
        char *message;
        char *label_key;
        struct SearchMessage smsg;
        struct SearchCondition *conditions;
        int  nconds;
        enum PARAM_TYPE type;
};

// Construct search params from zabbix agent reauest
struct SearchParams* set_log_search_params(char **params, int nparam, char *msg);
struct SearchParams* set_numeric_get_params(char **params, int nparam, char *msg);
struct SearchParams* set_discovery_params(char **params, int nparam, char *msg);

// Free search params
void free_sp(struct SearchParams *sp);

// Get hash name for search params
int construct_item_name(struct SearchParams *sp, char *name);

#endif
