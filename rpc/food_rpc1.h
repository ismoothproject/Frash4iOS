#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

void rpcserve(int rpcfd, void (*error)(int, int));

int set_sekrit(int rpcfd, void *sekrit, size_t sekrit_len);
struct set_sekrit_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	size_t sekrit_len;
	char extra[0];
} __attribute__((packed));
int abort_msg(int rpcfd, void *message, size_t message_len);
struct abort_msg_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	size_t message_len;
	char extra[0];
} __attribute__((packed));
struct abort_msg_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	char extra[0];
} __attribute__((packed));
int use_surface(int rpcfd, int name);
struct use_surface_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int name;
	char extra[0];
} __attribute__((packed));
int display_sync(int rpcfd, int l, int t, int r, int b);
struct display_sync_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int l;
	int t;
	int r;
	int b;
	char extra[0];
} __attribute__((packed));
int get_parameters(int rpcfd, void **params, size_t *params_len, int *params_count);
struct get_parameters_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	char extra[0];
} __attribute__((packed));
struct get_parameters_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	size_t params_len;
	int params_count;
	char extra[0];
} __attribute__((packed));
int new_get_connection(int rpcfd, stream_t stream, void *url, size_t url_len, void *target, size_t target_len, void **url_abs, size_t *url_abs_len);
struct new_get_connection_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	stream_t stream;
	size_t url_len;
	size_t target_len;
	char extra[0];
} __attribute__((packed));
struct new_get_connection_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	size_t url_abs_len;
	char extra[0];
} __attribute__((packed));
int new_post_connection(int rpcfd, stream_t stream, void *url, size_t url_len, void *target, size_t target_len, bool isfile, void *data, size_t data_len, void **url_abs, size_t *url_abs_len);
struct new_post_connection_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	stream_t stream;
	size_t url_len;
	size_t target_len;
	bool isfile;
	size_t data_len;
	char extra[0];
} __attribute__((packed));
struct new_post_connection_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	size_t url_abs_len;
	char extra[0];
} __attribute__((packed));
int get_window_object(int rpcfd, int *obj);
struct get_window_object_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	char extra[0];
} __attribute__((packed));
struct get_window_object_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	int obj;
	char extra[0];
} __attribute__((packed));
int evaluate_web_script(int rpcfd, void *script, size_t script_len, int *obj);
struct evaluate_web_script_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	size_t script_len;
	char extra[0];
} __attribute__((packed));
struct evaluate_web_script_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	int obj;
	char extra[0];
} __attribute__((packed));
int get_object_property(int rpcfd, int obj, void *property, size_t property_len, int *obj2);
struct get_object_property_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int obj;
	size_t property_len;
	char extra[0];
} __attribute__((packed));
struct get_object_property_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	int obj2;
	char extra[0];
} __attribute__((packed));
int get_string_object(int rpcfd, void *string, size_t string_len, int *obj2);
struct get_string_object_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	size_t string_len;
	char extra[0];
} __attribute__((packed));
struct get_string_object_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	int obj2;
	char extra[0];
} __attribute__((packed));
int get_int_object(int rpcfd, int theint, int *obj2);
struct get_int_object_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int theint;
	char extra[0];
} __attribute__((packed));
struct get_int_object_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	int obj2;
	char extra[0];
} __attribute__((packed));
int invoke_object_property(int rpcfd, int obj, void *property, size_t property_len, void *args, size_t args_len, int *obj2);
struct invoke_object_property_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int obj;
	size_t property_len;
	size_t args_len;
	char extra[0];
} __attribute__((packed));
struct invoke_object_property_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	int obj2;
	char extra[0];
} __attribute__((packed));
int get_string_value(int rpcfd, int obj, bool *valid, void **value, size_t *value_len);
struct get_string_value_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int obj;
	char extra[0];
} __attribute__((packed));
struct get_string_value_resp {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int status;
	bool valid;
	size_t value_len;
	char extra[0];
} __attribute__((packed));
int set_movie_size(int rpcfd, int w, int h);
struct set_movie_size_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int w;
	int h;
	char extra[0];
} __attribute__((packed));
int touch(int rpcfd, int action, int w, int h);
struct touch_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	int action;
	int w;
	int h;
	char extra[0];
} __attribute__((packed));
int connection_response(int rpcfd, stream_t stream, void *headers, size_t headers_len, int64_t expected_content_length);
struct connection_response_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	stream_t stream;
	size_t headers_len;
	int64_t expected_content_length;
	char extra[0];
} __attribute__((packed));
int connection_got_data(int rpcfd, stream_t stream, void *data, size_t data_len);
struct connection_got_data_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	stream_t stream;
	size_t data_len;
	char extra[0];
} __attribute__((packed));
int connection_all_done(int rpcfd, stream_t stream, bool completed);
struct connection_all_done_req {
	int magic;
	size_t msgsize;
	int is_resp;
	int msgid;
	int funcid;
	stream_t stream;
	bool completed;
	char extra[0];
} __attribute__((packed));
