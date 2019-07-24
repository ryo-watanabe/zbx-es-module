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
|message|required|Virtical bar separated messages to search log lines. Set * for all.|err&#124;warn&#124;fail|
|condition1|optional|Add label at start of log lines by @item_name. Also filtering by the item exists.|@container_name|
|condition2|optional|Filtering by item=value|hostname=cluster01m1|
|:|optional|Filetring by item exists (or not)|ignore_alerts (!ignore_alerts)|
|conditionN|optional|Item hierarchy expressed by dot|kubernetes.container_name=nginx|

example:  
```
es.log_search[2m,es.local:9200,kube_cluster,log,err|warn|fail,hostname=cluster02w1,log_name=/var/log/syslog]
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
|item_key|required|Key for numeric item|cpu_num|
|condition1|optional|Filtering by item=value|container=nginx|
|:|optional|(Item hierarchy expressed by dot)|fluentbit.tag=pod_metrics|
|conditionN|optional||||

example:  
```
es.uint[2m,es.local:9200,kube_cluster,cpu_idle,hostname=cluster02node01]
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
