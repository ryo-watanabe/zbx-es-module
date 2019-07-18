#ifndef ES_SEARCH_H
#define ES_SEARCH_H

#include "es_common.h"
#include "es_params.h"

// Log search through ES
int es_search(char **logs, struct SearchParams *sp, char *msg);

#endif
