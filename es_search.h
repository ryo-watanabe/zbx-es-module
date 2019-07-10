#ifndef ES_SEARCH_H
#define ES_SEARCH_H

enum CONDITION_TYPE {
        ITEM_IS_THE_VALUE,
        ITEM_EXISTS,
        ITEM_NOT_EXIST,
        ITEM_LABEL
};

struct SearchCondition {
        enum CONDITION_TYPE type;
        char item[32];
        char value[32];
};

struct SearchMessage {
        int nmsg;
        char *msg[10];
        char buf[256];
};

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

int es_search(char **logs, struct SearchParams *sp, char *msg);
struct SearchParams* set_params(char **params, int nparam, char *msg);

#define MESSAGE_MAX 256

#endif
