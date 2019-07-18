#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <curl/easy.h>

#include "es_common.h"
#include "curl_post.h"

// callback for curl write function
size_t buffer_writer(char *ptr, size_t size, size_t nmemb, void *stream) {

        struct Buffer *buf = (struct Buffer *)stream;
        int block = size * nmemb;

        if (!buf) {
                return block;
        }

        // allocate buffer for string length (+ 1 for '\0')
        if (!buf->data) {
                buf->data = (char *)malloc(block + 1);
        } else {
                buf->data = (char *)realloc(buf->data, buf->data_size + block + 1);
        }

        // copy into buffer and close string
        if (buf->data) {
                memcpy(buf->data + buf->data_size, ptr, block);
                buf->data_size += block;
                buf->data[buf->data_size] = '\0';
        }

        return block;
}

// Do curl POST
int curl_post(char* url, char* body, struct Buffer *buf, char* msg, long *code) {

        CURL *curl;
        struct curl_slist *headers = NULL;
        int curl_error = 0;

        // set http header
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // curl setup
        curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, buffer_writer);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_POST, 1);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(body));

        // curl execute
        CURLcode ret = curl_easy_perform(curl);
        //long code = 0;
        if (ret == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, code);
        } else {
                zbx_strlcpy(msg, curl_easy_strerror(ret), MESSAGE_MAX);
                curl_error = 1;
        }
        curl_easy_cleanup(curl);

        return curl_error;
}
