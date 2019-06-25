# zbx-es-module
Zabbix loadable module for ES

## Item keys

### es.log_search

es.log_search[es_endpoint,index_prefix,item_key,condition1,condition2,...,conditionN]

|param|for ..|examples|
|---|---|---|
|es_endpoint|ES host address and port|elasticsearch.local:9200|
|index_prefix|ES index prefix|Set "kube_cluster" for indices <br>kube_cluster-YYYY.MM.DD<br>(kube_cluster-2029.07.01,<br>kube_cluster-2029.07.02,<br>...)|
|item_key|Key for log item|log|
|message|Virtical bar (&#124;) separated messages to search log lines to submit. Set * for all.|err&#124;warn&#124;fail|
|condition1|Filtering by item=value|hostname=cluster01m1|
|:|Filetring by item exists (or not)|ignore_alerts (!ignore_alerts)|
|conditionN|Item hierarchy expressed by dot|kubernetes.container_name=nginx|
