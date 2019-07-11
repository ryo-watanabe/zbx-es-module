#ifndef CURL_POST_H
#define CURL_POST_H

// buffer struct for curl write data
struct Buffer {
        char *data;
        int data_size;
};

// callback for curl write function
size_t buffer_writer(char *ptr, size_t size, size_t nmemb, void *stream);

// Do curl post
int curl_post(char* url, char* body, struct Buffer *buf, char* msg, long *code);

#endif
