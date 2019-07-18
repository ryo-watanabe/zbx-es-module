#ifndef ES_PARAMS_H
#define ES_PARAMS_H

// Condition types for query
enum CONDITION_TYPE {
        ITEM_IS_THE_VALUE,
        ITEM_EXISTS,
        ITEM_NOT_EXIST,
        ITEM_LABEL
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
        char *msg[10];
        char buf[256];
};

// Parameter holder for ES search query
struct SearchParams {
        char *period;
        char *endpoint;
        char *prefix;
        char *item_key;
        char *message;
        char *label_key;
        struct SearchMessage smsg;
        struct SearchCondition *conditions;
        int  nconds;
};

// Construct search params from zabbix agent reauest
struct SearchParams* set_params(char **params, int nparam, char *msg);

// Free search params
void free_sp(struct SearchParams *sp);

// Get hash name for search params
int construct_item_name(struct SearchParams *sp, char *name);

#endif
