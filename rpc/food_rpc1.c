#include "food_rpc1.h"

#include <sys/socket.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <signal.h>
#include <libkern/OSAtomic.h>
#include <fcntl.h>

struct hdr {
    int magic;
    size_t msgsize;
    int is_resp;
    int msgid;
    int funcid_or_status;
    char extra[0];
} __attribute__((packed));

static int msgids;
static CFMutableDictionaryRef rpc_states;
static bool rpcserve_alive;
static pthread_mutex_t rpc_states_mutex = PTHREAD_MUTEX_INITIALIZER;
static CFMutableArrayRef pending_sends;
// Unnecessary?
static pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t send_cond = PTHREAD_COND_INITIALIZER;
static pthread_once_t send_once = PTHREAD_ONCE_INIT;

__attribute__((constructor))
static void init_rpc_states() {
    rpc_states = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
}

// Why can't I just have an unlimited buffer size? (whine, whine)

static void *xsend_thread(void *blah) {
    pthread_mutex_lock(&send_mutex);
    while(1) {
        struct hdr *hdr;
        while(1) {
            if(!CFArrayGetCount(pending_sends)) {
                break;
            }
            hdr = (void *) CFArrayGetValueAtIndex(pending_sends, 0);
            CFArrayRemoveValueAtIndex(pending_sends, 0);
            pthread_mutex_unlock(&send_mutex);
            int fd = hdr->magic;
            hdr->magic = 0x12345678;
            const char *p = (void *) hdr;
            size_t len = hdr->msgsize;
            //fprintf(stderr, "Ok I'm sending %p.  It has %d bytes of data\n", hdr, (int) len);
            while(len) {
                int bs = send(fd, p, len, 0);
                //fprintf(stderr, "bs = %d\n", bs);
                if(bs < 0) {
                    fprintf(stderr, "ERROR in xsend: %s\n", strerror(errno));
                    break;
                }
                len -= bs;
                p += bs;
            }
            free(hdr);
            pthread_mutex_lock(&send_mutex);
        }
        pthread_cond_wait(&send_cond, &send_mutex);
    }
    pthread_mutex_unlock(&send_mutex);
    return NULL;
}

static void xsend_startup() {
    pending_sends = CFArrayCreateMutable(NULL, 0, NULL);
    pthread_t thread;
    pthread_create(&thread, NULL, xsend_thread, NULL);
}

static int xsend(void *buf) {
    pthread_once(&send_once, xsend_startup);
    pthread_mutex_lock(&send_mutex);
    CFArrayAppendValue(pending_sends, buf);
    pthread_cond_signal(&send_cond);
    pthread_mutex_unlock(&send_mutex);
    return 0;
}



int set_sekrit(int rpcfd, void *sekrit, size_t sekrit_len) {
	size_t size = sizeof(struct set_sekrit_req) + sekrit_len;
	struct set_sekrit_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 1;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->sekrit_len = sekrit_len;
	char *extrap = req->extra;
	memcpy(extrap, sekrit, sekrit_len);
	extrap += sekrit_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	return 0;
}

int abort_msg(int rpcfd, void *message, size_t message_len) {
	size_t size = sizeof(struct abort_msg_req) + message_len;
	struct abort_msg_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 2;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->message_len = message_len;
	char *extrap = req->extra;
	memcpy(extrap, message, message_len);
	extrap += message_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct abort_msg_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg)) { free(s.msg); return 3; }
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int use_surface(int rpcfd, int name) {
	size_t size = sizeof(struct use_surface_req);
	struct use_surface_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 3;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->name = name;
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	return 0;
}

int display_sync(int rpcfd, int l, int t, int r, int b) {
	size_t size = sizeof(struct display_sync_req);
	struct display_sync_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 4;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->l = l;
	req->t = t;
	req->r = r;
	req->b = b;
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	return 0;
}

int get_parameters(int rpcfd, void **params, size_t *params_len, int *params_count) {
	size_t size = sizeof(struct get_parameters_req);
	struct get_parameters_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 5;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct get_parameters_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg) + s.msg->params_len) { free(s.msg); return 3; }
	if(params_len) *params_len = s.msg->params_len;
	if(params) {
		*params = malloc(s.msg->params_len + 1);
		*((char *)(*params) + s.msg->params_len) = 0;
		memcpy(*params, extrap, s.msg->params_len);
	}
	extrap += s.msg->params_len;
	if(params_count) *params_count = s.msg->params_count;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int new_get_connection(int rpcfd, stream_t stream, void *url, size_t url_len, void *target, size_t target_len, void **url_abs, size_t *url_abs_len) {
	size_t size = sizeof(struct new_get_connection_req) + url_len + target_len;
	struct new_get_connection_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 6;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->stream = stream;
	req->url_len = url_len;
	req->target_len = target_len;
	char *extrap = req->extra;
	memcpy(extrap, url, url_len);
	extrap += url_len;
	memcpy(extrap, target, target_len);
	extrap += target_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct new_get_connection_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg) + s.msg->url_abs_len) { free(s.msg); return 3; }
	if(url_abs_len) *url_abs_len = s.msg->url_abs_len;
	if(url_abs) {
		*url_abs = malloc(s.msg->url_abs_len + 1);
		*((char *)(*url_abs) + s.msg->url_abs_len) = 0;
		memcpy(*url_abs, extrap, s.msg->url_abs_len);
	}
	extrap += s.msg->url_abs_len;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int new_post_connection(int rpcfd, stream_t stream, void *url, size_t url_len, void *target, size_t target_len, bool isfile, void *data, size_t data_len, void **url_abs, size_t *url_abs_len) {
	size_t size = sizeof(struct new_post_connection_req) + url_len + target_len + data_len;
	struct new_post_connection_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 7;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->stream = stream;
	req->url_len = url_len;
	req->target_len = target_len;
	req->isfile = isfile;
	req->data_len = data_len;
	char *extrap = req->extra;
	memcpy(extrap, url, url_len);
	extrap += url_len;
	memcpy(extrap, target, target_len);
	extrap += target_len;
	memcpy(extrap, data, data_len);
	extrap += data_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct new_post_connection_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg) + s.msg->url_abs_len) { free(s.msg); return 3; }
	if(url_abs_len) *url_abs_len = s.msg->url_abs_len;
	if(url_abs) {
		*url_abs = malloc(s.msg->url_abs_len + 1);
		*((char *)(*url_abs) + s.msg->url_abs_len) = 0;
		memcpy(*url_abs, extrap, s.msg->url_abs_len);
	}
	extrap += s.msg->url_abs_len;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int get_window_object(int rpcfd, int *obj) {
	size_t size = sizeof(struct get_window_object_req);
	struct get_window_object_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 8;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct get_window_object_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg)) { free(s.msg); return 3; }
	if(obj) *obj = s.msg->obj;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int evaluate_web_script(int rpcfd, void *script, size_t script_len, int *obj) {
	size_t size = sizeof(struct evaluate_web_script_req) + script_len;
	struct evaluate_web_script_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 9;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->script_len = script_len;
	char *extrap = req->extra;
	memcpy(extrap, script, script_len);
	extrap += script_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct evaluate_web_script_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg)) { free(s.msg); return 3; }
	if(obj) *obj = s.msg->obj;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int get_object_property(int rpcfd, int obj, void *property, size_t property_len, int *obj2) {
	size_t size = sizeof(struct get_object_property_req) + property_len;
	struct get_object_property_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 10;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->obj = obj;
	req->property_len = property_len;
	char *extrap = req->extra;
	memcpy(extrap, property, property_len);
	extrap += property_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct get_object_property_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg)) { free(s.msg); return 3; }
	if(obj2) *obj2 = s.msg->obj2;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int get_string_object(int rpcfd, void *string, size_t string_len, int *obj2) {
	size_t size = sizeof(struct get_string_object_req) + string_len;
	struct get_string_object_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 11;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->string_len = string_len;
	char *extrap = req->extra;
	memcpy(extrap, string, string_len);
	extrap += string_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct get_string_object_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg)) { free(s.msg); return 3; }
	if(obj2) *obj2 = s.msg->obj2;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int get_int_object(int rpcfd, int theint, int *obj2) {
	size_t size = sizeof(struct get_int_object_req);
	struct get_int_object_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 12;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->theint = theint;
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct get_int_object_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg)) { free(s.msg); return 3; }
	if(obj2) *obj2 = s.msg->obj2;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int invoke_object_property(int rpcfd, int obj, void *property, size_t property_len, void *args, size_t args_len, int *obj2) {
	size_t size = sizeof(struct invoke_object_property_req) + property_len + args_len;
	struct invoke_object_property_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 13;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->obj = obj;
	req->property_len = property_len;
	req->args_len = args_len;
	char *extrap = req->extra;
	memcpy(extrap, property, property_len);
	extrap += property_len;
	memcpy(extrap, args, args_len);
	extrap += args_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct invoke_object_property_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg)) { free(s.msg); return 3; }
	if(obj2) *obj2 = s.msg->obj2;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}

int get_string_value(int rpcfd, int obj, bool *valid, void **value, size_t *value_len) {
	size_t size = sizeof(struct get_string_value_req);
	struct get_string_value_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 14;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->obj = obj;
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	if(!rpcserve_alive) {
		pthread_mutex_unlock(&rpc_states_mutex);
		free(req);
		return 2;
	}
	struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; struct get_string_value_resp *msg; } s;
	pthread_mutex_init(&s.mut, NULL);
	pthread_cond_init(&s.cond, NULL);
	s.msg = NULL;
	s.src_fd = rpcfd;
	CFDictionarySetValue(rpc_states, (void *) req->msgid, (void *) &s);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	pthread_mutex_lock(&s.mut);
	while(!s.msg) {
		pthread_cond_wait(&s.cond, &s.mut);
	}
	if(s.msg->status) {
		int status = s.msg->status;
		free(s.msg);
		return status;
	}
	extrap = s.msg->extra;
	if(s.msg->msgsize < sizeof(*s.msg) || s.msg->msgsize < sizeof(*s.msg) + s.msg->value_len) { free(s.msg); return 3; }
	if(valid) *valid = s.msg->valid;
	if(value_len) *value_len = s.msg->value_len;
	if(value) {
		*value = malloc(s.msg->value_len + 1);
		*((char *)(*value) + s.msg->value_len) = 0;
		memcpy(*value, extrap, s.msg->value_len);
	}
	extrap += s.msg->value_len;
	pthread_cond_destroy(&s.cond);
	pthread_mutex_destroy(&s.mut);
	free(s.msg);
	return 0;
}



int rpcserve_thread_(int, CFRunLoopSourceRef, pthread_mutex_t *, CFMutableArrayRef);

static void rpc_applier(const void *key, const void *value, void *context) {
    struct hdr *fake_resp = malloc(sizeof(struct hdr));
    fake_resp->magic = 0x12345678;
    fake_resp->is_resp = true;
    fake_resp->msgid = (int) key;
    fake_resp->funcid_or_status = 2;
    struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; void *msg; } *s = (void *) value;
    s->msg = fake_resp;
    pthread_mutex_lock(&s->mut);
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mut);
}

struct rpcserve_info {
    int rpcfd;
    void (*error)(int, int);
};


struct rpcctx_info {
    pthread_mutex_t mut;
    pthread_mutex_t bigmut; // So that the context is not destroyed before rpcctx_perform is done
    CFMutableArrayRef messages;
    int rpcfd;
};

void rpcctx_perform(void *info_) {
    struct rpcctx_info *info = info_;
    pthread_mutex_lock(&info->bigmut);
    int rpcfd = info->rpcfd;
    while(1) {
        pthread_mutex_lock(&info->mut);
        if(!CFArrayGetCount(info->messages)) {
            pthread_mutex_unlock(&info->mut);
            break;
        }
        
        struct hdr *msg_ = (void *) CFArrayGetValueAtIndex(info->messages, 0);
        CFArrayRemoveValueAtIndex(info->messages, 0);
        pthread_mutex_unlock(&info->mut);
        switch(msg_->funcid_or_status) {
            case 15: {
                struct set_movie_size_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    int _status = set_movie_size(rpcfd, msg->w, msg->h);
                }
            } break;
            
            case 16: {
                struct touch_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    int _status = touch(rpcfd, msg->action, msg->w, msg->h);
                }
            } break;
            
            case 17: {
                struct connection_response_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->headers_len) {
                    char *extrap = msg->extra;
                    char *headers = malloc(1 + msg->headers_len);
                    memcpy(headers, extrap, msg->headers_len);
                    headers[msg->headers_len] = 0;
                    extrap += msg->headers_len;
                    int _status = connection_response(rpcfd, msg->stream, headers, msg->headers_len, msg->expected_content_length);
                }
            } break;
            
            case 18: {
                struct connection_got_data_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->data_len) {
                    char *extrap = msg->extra;
                    char *data = malloc(1 + msg->data_len);
                    memcpy(data, extrap, msg->data_len);
                    data[msg->data_len] = 0;
                    extrap += msg->data_len;
                    int _status = connection_got_data(rpcfd, msg->stream, data, msg->data_len);
                }
            } break;
            
            case 19: {
                struct connection_all_done_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    int _status = connection_all_done(rpcfd, msg->stream, msg->completed);
                }
            } break;
            
            
        }
        pthread_mutex_destroy(&info->mut);
        free(msg_);
    }
    pthread_mutex_unlock(&info->bigmut);
}

struct rpcerrctx_info {
    int rpcfd;
    CFRunLoopSourceRef src;
    int ret;
    void (*error)(int, int);
};

void rpcerrctx_perform(void *info_) {
    struct rpcerrctx_info *info = info_;
    CFRunLoopSourceInvalidate(info->src);
    CFRelease(info->src);
    info->error(info->rpcfd, info->ret);

    free(info);
}

void *rpcserve_thread(void *info_) {
    struct rpcserve_info *info = info_;
    pthread_mutex_lock(&rpc_states_mutex);
    rpcserve_alive = true;
    pthread_mutex_unlock(&rpc_states_mutex);


    CFRunLoopSourceContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    struct rpcctx_info *rpcinfo = malloc(sizeof(*rpcinfo));
    ctx.info = rpcinfo;
    ctx.perform = rpcctx_perform;

    pthread_mutex_init(&rpcinfo->mut, NULL);
    pthread_mutex_init(&rpcinfo->bigmut, NULL);
    rpcinfo->rpcfd = info->rpcfd;
    rpcinfo->messages = CFArrayCreateMutable(NULL, 0, NULL);

    CFRunLoopSourceRef asrc = CFRunLoopSourceCreate(NULL, 0, &ctx);
    CFRunLoopAddSource(CFRunLoopGetMain(), asrc, kCFRunLoopCommonModes);


    int ret = rpcserve_thread_(info->rpcfd, asrc, &rpcinfo->mut, rpcinfo->messages);
    
    CFRunLoopSourceInvalidate(asrc);
    CFRelease(asrc);

    // Mark failure
    pthread_mutex_lock(&rpc_states_mutex);
    rpcserve_alive = false;
    pthread_mutex_unlock(&rpc_states_mutex);
    CFDictionaryApplyFunction(rpc_states, rpc_applier, NULL);
    if(info->error) {
        CFRunLoopSourceContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        struct rpcerrctx_info *errinfo = malloc(sizeof(*errinfo));
        ctx.info = errinfo;
        ctx.perform = rpcerrctx_perform;
        CFRunLoopSourceRef src = CFRunLoopSourceCreate(NULL, 0, &ctx);
        errinfo->src = src;
        errinfo->rpcfd = info->rpcfd;
        errinfo->ret = ret;
        errinfo->error = info->error;
        CFRunLoopAddSource(CFRunLoopGetMain(), src, kCFRunLoopCommonModes);
        CFRunLoopSourceSignal(src);
        CFRunLoopWakeUp(CFRunLoopGetMain());
    }
    pthread_mutex_lock(&rpcinfo->bigmut);
    pthread_mutex_unlock(&rpcinfo->bigmut);
    pthread_mutex_destroy(&rpcinfo->bigmut);
    pthread_mutex_destroy(&rpcinfo->mut);
    CFRelease(rpcinfo->messages);
    free(rpcinfo);
    free(info);
    return NULL;
}

void rpcserve(int rpcfd, void (*error)(int, int)) {
    pthread_t thread;
    struct rpcserve_info *info = malloc(sizeof(*info));
    info->rpcfd = rpcfd;
    info->error = error;
    pthread_create(&thread, NULL, rpcserve_thread, info);
}

int rpcserve_thread_(int rpcfd, CFRunLoopSourceRef src, pthread_mutex_t *mut, CFMutableArrayRef array) {
    while(1) {
        struct hdr hdr;
        if(recv(rpcfd, &hdr, sizeof(hdr), MSG_WAITALL) != sizeof(hdr)) {
            if(errno == EAGAIN) continue;
            return -errno;
        }
        if(hdr.magic != 0x12345678) {
            fprintf(stderr, "WARNING WARNING: bad magic\n");
            return 1;
        }
        //fprintf(stderr, "msgsize = %d\n", hdr.msgsize);
        if(hdr.msgsize < sizeof(hdr)) {
            fprintf(stderr, "WARNING WARNING: tiny message (%zd)\n", hdr.msgsize);
            return 2;
        }
        if(hdr.msgsize >= 1048576*50) return 3;
        struct hdr *msg_ = malloc(hdr.msgsize);
        memcpy(msg_, &hdr, sizeof(hdr));
        if(recv(rpcfd, msg_->extra, hdr.msgsize - sizeof(*msg_), MSG_WAITALL) != hdr.msgsize - sizeof(*msg_)) return -errno;
        if(hdr.is_resp) {
            pthread_mutex_lock(&rpc_states_mutex);
            struct { pthread_cond_t cond; pthread_mutex_t mut; int src_fd; void *msg; } *s = \
            (void *) CFDictionaryGetValue(rpc_states, (void *) hdr.msgid);
            CFDictionaryRemoveValue(rpc_states, (void *) hdr.msgid);
            pthread_mutex_unlock(&rpc_states_mutex);
            //fprintf(stderr, "got a response for msgid %d (%p)\n", hdr.msgid, s);
            if(s && s->src_fd == rpcfd) {
                s->msg = msg_;
                pthread_mutex_lock(&s->mut);
                pthread_cond_signal(&s->cond);
                pthread_mutex_unlock(&s->mut);
            } else {
                free(msg_);
            }
            continue;
        }
        // This part (only) should add something to a run loop
        pthread_mutex_lock(mut);
        CFArrayAppendValue(array, msg_);
        pthread_mutex_unlock(mut);
        CFRunLoopSourceSignal(src);
        CFRunLoopWakeUp(CFRunLoopGetMain());
    }
}


