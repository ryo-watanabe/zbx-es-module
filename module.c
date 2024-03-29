/*
** Zabbix
** Copyright (C) 2001-2019 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "zabbix/sysinc.h"
#include "zabbix/module.h"
#include "zabbix/common.h"
#include "zabbix/log.h"
#include "db.h"
#include "es_params.h"
#include "es_search.h"

#define LOG_LEVEL_ES_MODULE LOG_LEVEL_DEBUG

/* the variable keeps timeout setting for item processing */
static int	item_timeout = 0;

/* module SHOULD define internal functions as static and use a naming pattern different from Zabbix internal */
/* symbols (zbx_*) and loadable module API functions (zbx_module_*) to avoid conflicts                       */
static int	dummy_ping(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	dummy_echo(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	dummy_random(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	es_log_search(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	es_uint_get(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	es_double_get(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	es_discovery_search(AGENT_REQUEST *request, AGENT_RESULT *result);

static ZBX_METRIC keys[] =
/*	KEY			FLAG		FUNCTION	TEST PARAMETERS */
{
	{"dummy.ping",		0,		dummy_ping,	NULL},
	{"dummy.echo",		CF_HAVEPARAMS,	dummy_echo,	"a message"},
	{"dummy.random",	CF_HAVEPARAMS,	dummy_random,	"1,1000"},
	{"es.log_search",	CF_HAVEPARAMS,	es_log_search,	NULL},
	{"es.uint",		CF_HAVEPARAMS,	es_uint_get,	NULL},
	{"es.double",		CF_HAVEPARAMS,	es_double_get,	NULL},
	{"es.discovery",	CF_HAVEPARAMS,	es_discovery_search,	NULL},
	{NULL}
};

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_api_version                                           *
 *                                                                            *
 * Purpose: returns version number of the module interface                    *
 *                                                                            *
 * Return value: ZBX_MODULE_API_VERSION - version of module.h module is       *
 *               compiled with, in order to load module successfully Zabbix   *
 *               MUST be compiled with the same version of this header file   *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_api_version(void)
{
	return ZBX_MODULE_API_VERSION;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_timeout                                          *
 *                                                                            *
 * Purpose: set timeout value for processing of items                         *
 *                                                                            *
 * Parameters: timeout - timeout in seconds, 0 - no timeout set               *
 *                                                                            *
 ******************************************************************************/
void	zbx_module_item_timeout(int timeout)
{
	item_timeout = timeout;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_item_list                                             *
 *                                                                            *
 * Purpose: returns list of item keys supported by the module                 *
 *                                                                            *
 * Return value: list of item keys                                            *
 *                                                                            *
 ******************************************************************************/
ZBX_METRIC	*zbx_module_item_list(void)
{
	zabbix_log(LOG_LEVEL_ES_MODULE, "module loading");
	return keys;
}

static int	es_log_search(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	zabbix_log(LOG_LEVEL_ES_MODULE, "es_log_search called");

	char msg[MESSAGE_MAX] = "";
	struct SearchParams *sp;
	if (NULL == (sp = set_log_search_params(request->params, request->nparam, msg))) {
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	char *logs;
	*msg = '\0';
	int ret = es_log(&logs, sp, msg);
	if (ret == 0) {
		SET_TEXT_RESULT(result, logs);
	} else if (ret != 2) { // ret = 2 means no logs.
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	return SYSINFO_RET_OK;
}

static int	es_uint_get(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	zabbix_log(LOG_LEVEL_ES_MODULE, "es_uint called");

	char msg[MESSAGE_MAX] = "";
	struct SearchParams *sp;
	if (NULL == (sp = set_numeric_get_params(request->params, request->nparam, msg))) {
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	unsigned long value = 0;
	*msg = '\0';
	int ret = es_uint(&value, sp, msg);
	if (ret == 0) {
		SET_UI64_RESULT(result, value);
	} else if (ret != 2) { // ret = 2 means no logs.
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	return SYSINFO_RET_OK;
}

static int	es_double_get(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	zabbix_log(LOG_LEVEL_ES_MODULE, "es_double called");

	char msg[MESSAGE_MAX] = "";
	struct SearchParams *sp;
	if (NULL == (sp = set_numeric_get_params(request->params, request->nparam, msg))) {
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	double value = 0;
	*msg = '\0';
	int ret = es_double(&value, sp, msg);
	if (ret == 0) {
		SET_DBL_RESULT(result, value);
	} else if (ret != 2) { // ret = 2 means no logs.
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	return SYSINFO_RET_OK;
}

static int	es_discovery_search(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	zabbix_log(LOG_LEVEL_ES_MODULE, "es_discovery_search called");

	char msg[MESSAGE_MAX] = "";
	struct SearchParams *sp;
	if (NULL == (sp = set_discovery_params(request->params, request->nparam, msg))) {
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	char *data;
	*msg = '\0';
	int ret = es_discovery(&data, sp, msg);
	if (ret == 0) {
		SET_STR_RESULT(result, data);
	} else {
		SET_MSG_RESULT(result, strdup(msg));
		return SYSINFO_RET_FAIL;
	}

	return SYSINFO_RET_OK;
}

static int	dummy_ping(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	zabbix_log(LOG_LEVEL_ES_MODULE, "dummy_ping called");

	SET_UI64_RESULT(result, 1);

	return SYSINFO_RET_OK;
}

static int	dummy_echo(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char	*param;

	zabbix_log(LOG_LEVEL_ES_MODULE, "dummy_echo called");

	if (1 != request->nparam)
	{
		/* set optional error message */
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		return SYSINFO_RET_FAIL;
	}

	param = get_rparam(request, 0);

	SET_STR_RESULT(result, strdup(param));

	return SYSINFO_RET_OK;
}

/******************************************************************************
 *                                                                            *
 * Function: dummy_random                                                     *
 *                                                                            *
 * Purpose: a main entry point for processing of an item                      *
 *                                                                            *
 * Parameters: request - structure that contains item key and parameters      *
 *              request->key - item key without parameters                    *
 *              request->nparam - number of parameters                        *
 *              request->timeout - processing should not take longer than     *
 *                                 this number of seconds                     *
 *              request->params[N-1] - pointers to item key parameters        *
 *                                                                            *
 *             result - structure that will contain result                    *
 *                                                                            *
 * Return value: SYSINFO_RET_FAIL - function failed, item will be marked      *
 *                                 as not supported by zabbix                 *
 *               SYSINFO_RET_OK - success                                     *
 *                                                                            *
 * Comment: get_rparam(request, N-1) can be used to get a pointer to the Nth  *
 *          parameter starting from 0 (first parameter). Make sure it exists  *
 *          by checking value of request->nparam.                             *
 *                                                                            *
 ******************************************************************************/
static int	dummy_random(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char	*param1, *param2;
	int	from, to;

	zabbix_log(LOG_LEVEL_ES_MODULE, "dummy_random called");

	if (2 != request->nparam)
	{
		/* set optional error message */
		SET_MSG_RESULT(result, strdup("Invalid number of parameters."));
		return SYSINFO_RET_FAIL;
	}

	param1 = get_rparam(request, 0);
	param2 = get_rparam(request, 1);

	/* there is no strict validation of parameters for simplicity sake */
	from = atoi(param1);
	to = atoi(param2);

	if (from > to)
	{
		SET_MSG_RESULT(result, strdup("Invalid range specified."));
		return SYSINFO_RET_FAIL;
	}

	SET_UI64_RESULT(result, from + rand() % (to - from + 1));

	return SYSINFO_RET_OK;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_init                                                  *
 *                                                                            *
 * Purpose: the function is called on agent startup                           *
 *          It should be used to call any initialization routines             *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - module initialization failed               *
 *                                                                            *
 * Comment: the module won't be loaded in case of ZBX_MODULE_FAIL             *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_init(void)
{
	/* initialization for dummy.random */
	srand(time(NULL));

	if (init_db()) {
		return ZBX_MODULE_FAIL;
	}

	return ZBX_MODULE_OK;
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_uninit                                                *
 *                                                                            *
 * Purpose: the function is called on agent shutdown                          *
 *          It should be used to cleanup used resources if there are any      *
 *                                                                            *
 * Return value: ZBX_MODULE_OK - success                                      *
 *               ZBX_MODULE_FAIL - function failed                            *
 *                                                                            *
 ******************************************************************************/
int	zbx_module_uninit(void)
{
	return ZBX_MODULE_OK;
}

/******************************************************************************
 *                                                                            *
 * Functions: dummy_history_float_cb                                          *
 *            dummy_history_integer_cb                                        *
 *            dummy_history_string_cb                                         *
 *            dummy_history_text_cb                                           *
 *            dummy_history_log_cb                                            *
 *                                                                            *
 * Purpose: callback functions for storing historical data of types float,    *
 *          integer, string, text and log respectively in external storage    *
 *                                                                            *
 * Parameters: history     - array of historical data                         *
 *             history_num - number of elements in history array              *
 *                                                                            *
 ******************************************************************************/
static void	dummy_history_float_cb(const ZBX_HISTORY_FLOAT *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	dummy_history_integer_cb(const ZBX_HISTORY_INTEGER *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	dummy_history_string_cb(const ZBX_HISTORY_STRING *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	dummy_history_text_cb(const ZBX_HISTORY_TEXT *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

static void	dummy_history_log_cb(const ZBX_HISTORY_LOG *history, int history_num)
{
	int	i;

	for (i = 0; i < history_num; i++)
	{
		/* do something with history[i].itemid, history[i].clock, history[i].ns, history[i].value, ... */
	}
}

/******************************************************************************
 *                                                                            *
 * Function: zbx_module_history_write_cbs                                     *
 *                                                                            *
 * Purpose: returns a set of module functions Zabbix will call to export      *
 *          different types of historical data                                *
 *                                                                            *
 * Return value: structure with callback function pointers (can be NULL if    *
 *               module is not interested in data of certain types)           *
 *                                                                            *
 ******************************************************************************/
ZBX_HISTORY_WRITE_CBS	zbx_module_history_write_cbs(void)
{
	static ZBX_HISTORY_WRITE_CBS	dummy_callbacks =
	{
		dummy_history_float_cb,
		dummy_history_integer_cb,
		dummy_history_string_cb,
		dummy_history_text_cb,
		dummy_history_log_cb,
	};

	return dummy_callbacks;
}
