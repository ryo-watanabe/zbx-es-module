# zbx-es-module
Zabbix loadable module for ES

## Item keys

### es.log_search

es.log_search[es_endpoint,index_prefix,item_key,condition1,condition2,...,conditionN]

|param|for ..|examples|
|---|---|---|
|es_endpoint|ES host address and port|elasticsearch.local:9200|
|index_prefix|ES index prefix|Set "kube_cluster" for indices <br>kube_cluster-YYYY.MM.DD<br>(kube_cluster-2029.07.01,<br>kube_cluster-2029.07.02,<br>...)|
