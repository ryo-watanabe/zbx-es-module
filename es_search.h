#ifndef ES_SEARCH_H
#define ES_SEARCH_H

enum CONDITION_TYPE {
        ITEM_IS_THE_VALUE,
        ITEM_EXISTS,
        ITEM_NOT_EXIST
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
        //char *message;
        struct SearchMessage smsg;
        struct SearchCondition *conditions;
        int  nconds;
};

char* es_search(struct SearchParams *sp);
struct SearchParams* set_params(char **params, int nparam, char *msg);

#endif
