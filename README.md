# zbx-es-module
Zabbix loadable module for watching/alerting metrics and logs in ES.

## Log Item
Get log lines in search range (= now-[period]s) newer then previous search, by query constructed with filtering message and conditions 1...N

key format:
```
es.log_search[<period>,<es_endpoint>,<index_prefix>,<item_key>,<message>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Search range in seconds, must longer than execution period.|Set 100 (search from now-100s to now) for execution period 60s.|
|es_endpoint|required|ES host address and port|elasticsearch.local:9200|
|index_prefix|required|ES index prefix|Set "kube_cluster" for indices <br>kube_cluster-YYYY.MM.DD<br>(kube_cluster-2029.07.01,<br>kube_cluster-2029.07.02,<br>...)|
|item_key|required|Key for log item|log|
|message|required|Virtical bar separated messages to search log lines. Set * to get all log lines.|error&#124;warn&#124;fail|
|condition1<br><br> : <br><br>conditionN|optional|- Filtering by item_name=value (String value can contain wildcards '*')<br>- Filetring by item exists<br>- Filetring by item not exists (!item)<br>-  Add label at start of log lines by @item_name. (Also filtered by the item exists)<br>- Item hierarchy expressed by dot|hostname=cluster01m1,@container_name<br><br>kubernetes.container_name=nginx,!ignore_alerts|

example:  
```
es.log_search[100,es.local:9200,kube_cluster,log,err|warn|fail,hostname=cluster02w1,log_name=/var/log/syslog]
```

## Numeric (float/unsigned) Item

Get newest numeric value in search range (= now-[period]s) by query constructed with conditions 1...N

key format:
```
es.uint[<period>,<es_endpoint>,<index_prefix>,<item_key>,<condition1>,<condition2>,...,<conditionN>]
es.double[<period>,<es_endpoint>,<index_prefix>,<item_key>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Search range in seconds, must longer than execution period.|Set 100 (search from now-100s to now) for execution period 60s.|
|es_endpoint|required|ES host address and port|elasticsearch.local:9200|
|index_prefix|required|ES index prefix|Set "kube_cluster" for indices <br>kube_cluster-YYYY.MM.DD<br>(kube_cluster-2029.07.01,<br>kube_cluster-2029.07.02,<br>...)|
|item_key|required|Key for numeric item|idle|
|condition1<br><br> : <br><br>conditionN|optional|- Filtering by item_name=value (String value can contain wildcards '*')<br>- Filetring by item exists<br>- Filetring by item not exists (!item)<br>- Item hierarchy expressed by dot|hostname=cluster01node01,fluentbit.tag=os.cpu|

example:  
```
es.uint[100,es.local:9200,kube_cluster,idle,hostname=cluster02node01,fluentbit.tag=os.cpu]
```

## Discovery Item

Get possible values of an item in zabbix LLD json format with conditions 1...N

key format:
```
es.discovery[<period>,<es_endpoint>,<index_prefix>,<item_key>,<macro>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Search range in days|2 = search from now-2d to now|
|es_endpoint|required|ES host address and port|elasticsearch.local:9200|
|index_prefix|required|ES index prefix|Set "kube_cluster" for indices <br>kube_cluster-YYYY.MM.DD<br>(kube_cluster-2029.07.01,<br>kube_cluster-2029.07.02,<br>...)|
|item_key|required|Key for discovery item|hostname.keyword|
|macro|required|Zabbix Macro for discovery item|Set "DSCV_HOST"<br>for zabbix macro {#DSCV_HOST}|
|condition1<br><br> : <br><br>conditionN|optional|- Filtering by item_name=value (String value can contain wildcards '*')<br>- Filetring by item exists<br>- Filetring by item not exists (!item)<br>- Item hierarchy expressed by dot|fluentbit.tag=apps.*|

example:  
```
es.discovery[2,es.local:9200,kube_cluster,hostname.keyword,DSCV_HOST]

es.discovery[2,es.local:9200,kube_cluster,name.keyword,DSCV_APP,fluentbit.tag=apps.*]
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
