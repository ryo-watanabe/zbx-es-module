#ifndef ES_SEARCH_H
#define ES_SEARCH_H

#include "es_common.h"
#include "es_params.h"

// Search types
enum SEARCH_TYPE {
        SEARCH_TYPE_LOG,
        SEARCH_TYPE_NUMBER
};

// Log search through ES
int es_log(char **logs, struct SearchParams *sp, char *msg);

// Get uint metrics through ES
int es_uint(unsigned long *val, struct SearchParams *sp, char *msg);

// Log double metrics through ES
int es_double(double *val, struct SearchParams *sp, char *msg);

#endif
