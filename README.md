# zbx-es-module
Zabbix loadable module for watching/alerting metrics and logs in ES.

## Log Item
Get log lines in search range (= now-[period]s) newer then previous search, by query constructed with filtering message and conditions 1...N

Key format:
```
es.log_search[<period>,<es_endpoint>,<es_index>,<item_key>,<message>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Search range in seconds, must longer than execution period.|Set 100 (search from now-100s to now) for execution period 60s.|
|es_endpoint|required|ES host address and port|elasticsearch.local:9200|
|es_index|required|ES index (with wildcard)|kube_cluster-* <br>for daily indices <br>kube_cluster-YYYY.MM.DD|
|item_key|required|Key for log item|log|
|message|required|Virtical bar separated messages to search log lines. Set * to get all log lines.|error&#124;warn&#124;fail|
|condition1<br> : <br>conditionN|optional|See filtering condition formats|hostname=nodeserver01<br>kubernetes.container_name=nginx<br>!ignore_alerts|

Filtering condition formats:

|filtering by|format|example|notes|
|---|---|---|---|
|Item value|[item]=[value]|hostname=nodeserver01|-|
|Item value except|![item]=[value]|!namespace=apptest|-|
|Item value with wildcard|[item]=[value with *]|tag=apps.*|-|
|Item exists|[item]|metrics.cpu|-|
|Item exists (add label)|@[item]|@container_name|- Log item only<br>Add label [value] at start of log lines|
|Item not exists|![item]|!ignore_alert|-|
* Item hierarchy expressed by dot, e.g. kubernetes.container_name.
* Item values are queried by [item].keyword from the module.

Key example:  
```
es.log_search[100,es.local:9200,kube_cluster-*,log,err|warn|fail,hostname=cluster02w1,log_name=/var/log/syslog]
```

## Numeric (float/unsigned) Item

Get newest numeric value in search range (= now-[period]s) by query constructed with conditions 1...N

Key format:
```
es.uint[<period>,<es_endpoint>,<es_index>,<item_key>,<condition1>,<condition2>,...,<conditionN>]
es.double[<period>,<es_endpoint>,<es_index>,<item_key>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Search range in seconds, must longer than execution period.|Set 100 (search from now-100s to now) for execution period 60s.|
|es_endpoint|required|ES host address and port|elasticsearch.local:9200|
|es_index|required|ES index (with wildcard)|kube_cluster-* <br>for daily indices <br>kube_cluster-YYYY.MM.DD|
|item_key|required|Key for numeric item|idle|
|condition1<br> : <br>conditionN|optional|See filtering condition formats of Log Item|hostname=cluster01node01<br>fluentbit.tag=os.cpu|

Key Example:  
```
es.uint[100,es.local:9200,kube_cluster-*,idle,hostname=cluster02node01,fluentbit.tag=os.cpu]
```

## Discovery Item

Get possible values of an item in zabbix LLD json format with conditions 1...N

Key format:
```
es.discovery[<period>,<es_endpoint>,<es_index>,<item_key>,<macro>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Search range in days|2 = search from now-2d to now|
|es_endpoint|required|ES host address and port|elasticsearch.local:9200|
|es_index|required|ES index (with wildcard)|kube_cluster-* <br>for daily indices <br>kube_cluster-YYYY.MM.DD|
|item_key|required|Key for discovery item|hostname|
|macro|required|Zabbix Macro for discovery item|Set "DSCV_HOST"<br>for zabbix macro {#DSCV_HOST}|
|condition1<br> : <br>conditionN|optional|See filtering condition formats of Log Item|fluentbit.tag=apps.*|

* Discovery values are queried by [item_key].keyword from the module.

Key example:  
```
es.discovery[2,es.local:9200,kube_cluster-*,hostname,DSCV_HOST]

es.discovery[2,es.local:9200,kube_cluster-*,name,DSCV_APP,fluentbit.tag=apps.*]
```

## Build & Config

### Dependency
- libcurl
- jansson
- sqlite
- openssl

### Build
```
gcc -fPIC -shared -o es_module.so *.c -lcurl -ljansson -lsqlite3 -lcrypto
```
