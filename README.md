# zbx-es-module
Zabbix loadable module for alerting metrics and logs in ES.

## Log Item
format:
```
es.log_search[<period>,<es_endpoint>,<index_prefix>,<item_key>,<message>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Search period in seconds, must longer than execution period.|Set 100 (search from now-100s to now) for execution period 60s.|
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

## Unsigned Int / Double Item
Not implemented yet.

format:
```
es.uint[<period>,<es_endpoint>,<index_prefix>,<item_key>,<condition1>,<condition2>,...,<conditionN>]
es.double[<period>,<es_endpoint>,<index_prefix>,<item_key>,<condition1>,<condition2>,...,<conditionN>]
```
|param|required/optional|for setting ..|examples|
|---|---|---|---|
|period|required|Execution period in seconds.|300 (= 5min.)|
|es_endpoint|required|ES host address and port|elasticsearch.local:9200|
|index_prefix|required|ES index prefix|Set "kube_cluster" for indices <br>kube_cluster-YYYY.MM.DD<br>(kube_cluster-2029.07.01,<br>kube_cluster-2029.07.02,<br>...)|
|item_key|required|Key for log item|cpu_idle|
|condition1|optional|Filtering by item=value|hostname=cluster01m1|
|:|optional|Item hierarchy expressed by dot|kubernetes.container_name=nginx|
|conditionN|optional||||

example:  
```
es.uint[2m,es.local:9200,kube_cluster,cpu_idle,hostname=cluster02w1]
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
