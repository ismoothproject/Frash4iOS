#include "food_rpc2.h"

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



int set_movie_size(int rpcfd, int w, int h) {
	size_t size = sizeof(struct set_movie_size_req);
	struct set_movie_size_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 15;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->w = w;
	req->h = h;
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	return 0;
}

int touch(int rpcfd, int action, int w, int h) {
	size_t size = sizeof(struct touch_req);
	struct touch_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 16;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->action = action;
	req->w = w;
	req->h = h;
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	return 0;
}

int connection_response(int rpcfd, stream_t stream, void *headers, size_t headers_len, int64_t expected_content_length) {
	size_t size = sizeof(struct connection_response_req) + headers_len;
	struct connection_response_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 17;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->stream = stream;
	req->headers_len = headers_len;
	req->expected_content_length = expected_content_length;
	char *extrap = req->extra;
	memcpy(extrap, headers, headers_len);
	extrap += headers_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	return 0;
}

int connection_got_data(int rpcfd, stream_t stream, void *data, size_t data_len) {
	size_t size = sizeof(struct connection_got_data_req) + data_len;
	struct connection_got_data_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 18;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->stream = stream;
	req->data_len = data_len;
	char *extrap = req->extra;
	memcpy(extrap, data, data_len);
	extrap += data_len;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
	return 0;
}

int connection_all_done(int rpcfd, stream_t stream, bool completed) {
	size_t size = sizeof(struct connection_all_done_req);
	struct connection_all_done_req *req = malloc(size);
	req->magic = rpcfd;
	req->msgsize = size;
	req->is_resp = false;
	req->msgid = OSAtomicIncrement32(&msgids);
	req->funcid = 19;
	//fprintf(stderr, "funcid = %d\n", req->funcid);
	req->stream = stream;
	req->completed = completed;
	char *extrap = req->extra;
	signal(SIGPIPE, SIG_IGN);
	ssize_t bs;
	pthread_mutex_lock(&rpc_states_mutex);
	pthread_mutex_unlock(&rpc_states_mutex);
	if(xsend(req)) { return 1; };
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
            case 1: {
                struct set_sekrit_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->sekrit_len) {
                    char *extrap = msg->extra;
                    char *sekrit = malloc(1 + msg->sekrit_len);
                    memcpy(sekrit, extrap, msg->sekrit_len);
                    sekrit[msg->sekrit_len] = 0;
                    extrap += msg->sekrit_len;
                    int _status = set_sekrit(rpcfd, sekrit, msg->sekrit_len);
                }
            } break;
            
            case 2: {
                struct abort_msg_req *msg = (void *) msg_;
                struct abort_msg_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->message_len) {
                    char *extrap = msg->extra;
                    char *message = malloc(1 + msg->message_len);
                    memcpy(message, extrap, msg->message_len);
                    message[msg->message_len] = 0;
                    extrap += msg->message_len;
                    int _status = abort_msg(rpcfd, message, msg->message_len);
                    size_t size = sizeof(struct abort_msg_resp);
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct abort_msg_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    xsend(resp);
                }
            } break;
            
            case 3: {
                struct use_surface_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    int _status = use_surface(rpcfd, msg->name);
                }
            } break;
            
            case 4: {
                struct display_sync_req *msg = (void *) msg_;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    int _status = display_sync(rpcfd, msg->l, msg->t, msg->r, msg->b);
                }
            } break;
            
            case 5: {
                struct get_parameters_req *msg = (void *) msg_;
                struct get_parameters_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                    sresp.params_len = 0x7fffffff;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    void *params;
                    int _status = get_parameters(rpcfd, &params, &sresp.params_len, &sresp.params_count);
                    assert(sresp.params_len != 0x7fffffff);
                    size_t size = sizeof(struct get_parameters_resp) + sresp.params_len;
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct get_parameters_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    if(params) memcpy(extrap, params, resp->params_len);
                    extrap += resp->params_len;
                    xsend(resp);
                }
            } break;
            
            case 6: {
                struct new_get_connection_req *msg = (void *) msg_;
                struct new_get_connection_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                    sresp.url_abs_len = 0x7fffffff;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->url_len + msg->target_len) {
                    char *extrap = msg->extra;
                    char *url = malloc(1 + msg->url_len);
                    memcpy(url, extrap, msg->url_len);
                    url[msg->url_len] = 0;
                    extrap += msg->url_len;
                    char *target = malloc(1 + msg->target_len);
                    memcpy(target, extrap, msg->target_len);
                    target[msg->target_len] = 0;
                    extrap += msg->target_len;
                    void *url_abs;
                    int _status = new_get_connection(rpcfd, msg->stream, url, msg->url_len, target, msg->target_len, &url_abs, &sresp.url_abs_len);
                    assert(sresp.url_abs_len != 0x7fffffff);
                    size_t size = sizeof(struct new_get_connection_resp) + sresp.url_abs_len;
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct new_get_connection_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    if(url_abs) memcpy(extrap, url_abs, resp->url_abs_len);
                    extrap += resp->url_abs_len;
                    xsend(resp);
                }
            } break;
            
            case 7: {
                struct new_post_connection_req *msg = (void *) msg_;
                struct new_post_connection_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                    sresp.url_abs_len = 0x7fffffff;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->url_len + msg->target_len + msg->data_len) {
                    char *extrap = msg->extra;
                    char *url = malloc(1 + msg->url_len);
                    memcpy(url, extrap, msg->url_len);
                    url[msg->url_len] = 0;
                    extrap += msg->url_len;
                    char *target = malloc(1 + msg->target_len);
                    memcpy(target, extrap, msg->target_len);
                    target[msg->target_len] = 0;
                    extrap += msg->target_len;
                    char *data = malloc(1 + msg->data_len);
                    memcpy(data, extrap, msg->data_len);
                    data[msg->data_len] = 0;
                    extrap += msg->data_len;
                    void *url_abs;
                    int _status = new_post_connection(rpcfd, msg->stream, url, msg->url_len, target, msg->target_len, msg->isfile, data, msg->data_len, &url_abs, &sresp.url_abs_len);
                    assert(sresp.url_abs_len != 0x7fffffff);
                    size_t size = sizeof(struct new_post_connection_resp) + sresp.url_abs_len;
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct new_post_connection_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    if(url_abs) memcpy(extrap, url_abs, resp->url_abs_len);
                    extrap += resp->url_abs_len;
                    xsend(resp);
                }
            } break;
            
            case 8: {
                struct get_window_object_req *msg = (void *) msg_;
                struct get_window_object_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    int _status = get_window_object(rpcfd, &sresp.obj);
                    size_t size = sizeof(struct get_window_object_resp);
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct get_window_object_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    xsend(resp);
                }
            } break;
            
            case 9: {
                struct evaluate_web_script_req *msg = (void *) msg_;
                struct evaluate_web_script_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->script_len) {
                    char *extrap = msg->extra;
                    char *script = malloc(1 + msg->script_len);
                    memcpy(script, extrap, msg->script_len);
                    script[msg->script_len] = 0;
                    extrap += msg->script_len;
                    int _status = evaluate_web_script(rpcfd, script, msg->script_len, &sresp.obj);
                    size_t size = sizeof(struct evaluate_web_script_resp);
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct evaluate_web_script_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    xsend(resp);
                }
            } break;
            
            case 10: {
                struct get_object_property_req *msg = (void *) msg_;
                struct get_object_property_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->property_len) {
                    char *extrap = msg->extra;
                    char *property = malloc(1 + msg->property_len);
                    memcpy(property, extrap, msg->property_len);
                    property[msg->property_len] = 0;
                    extrap += msg->property_len;
                    int _status = get_object_property(rpcfd, msg->obj, property, msg->property_len, &sresp.obj2);
                    size_t size = sizeof(struct get_object_property_resp);
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct get_object_property_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    xsend(resp);
                }
            } break;
            
            case 11: {
                struct get_string_object_req *msg = (void *) msg_;
                struct get_string_object_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->string_len) {
                    char *extrap = msg->extra;
                    char *string = malloc(1 + msg->string_len);
                    memcpy(string, extrap, msg->string_len);
                    string[msg->string_len] = 0;
                    extrap += msg->string_len;
                    int _status = get_string_object(rpcfd, string, msg->string_len, &sresp.obj2);
                    size_t size = sizeof(struct get_string_object_resp);
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct get_string_object_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    xsend(resp);
                }
            } break;
            
            case 12: {
                struct get_int_object_req *msg = (void *) msg_;
                struct get_int_object_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    int _status = get_int_object(rpcfd, msg->theint, &sresp.obj2);
                    size_t size = sizeof(struct get_int_object_resp);
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct get_int_object_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    xsend(resp);
                }
            } break;
            
            case 13: {
                struct invoke_object_property_req *msg = (void *) msg_;
                struct invoke_object_property_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg) + msg->property_len + msg->args_len) {
                    char *extrap = msg->extra;
                    char *property = malloc(1 + msg->property_len);
                    memcpy(property, extrap, msg->property_len);
                    property[msg->property_len] = 0;
                    extrap += msg->property_len;
                    char *args = malloc(1 + msg->args_len);
                    memcpy(args, extrap, msg->args_len);
                    args[msg->args_len] = 0;
                    extrap += msg->args_len;
                    int _status = invoke_object_property(rpcfd, msg->obj, property, msg->property_len, args, msg->args_len, &sresp.obj2);
                    size_t size = sizeof(struct invoke_object_property_resp);
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct invoke_object_property_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    xsend(resp);
                }
            } break;
            
            case 14: {
                struct get_string_value_req *msg = (void *) msg_;
                struct get_string_value_resp sresp;
                    sresp.msgid = msg->msgid;
                    sresp.is_resp = true;
                    sresp.value_len = 0x7fffffff;
                if(msg->msgsize >= sizeof(*msg) && msg->msgsize >= sizeof(*msg)) {
                    char *extrap = msg->extra;
                    void *value;
                    int _status = get_string_value(rpcfd, msg->obj, &sresp.valid, &value, &sresp.value_len);
                    assert(sresp.value_len != 0x7fffffff);
                    size_t size = sizeof(struct get_string_value_resp) + sresp.value_len;
                    sresp.magic = rpcfd;
                    sresp.msgsize = size;
                    sresp.status = _status;
                    struct get_string_value_resp *resp = malloc(size);
                    memcpy(resp, &sresp, sizeof(sresp));
                    extrap = resp->extra;
                    if(value) memcpy(extrap, value, resp->value_len);
                    extrap += resp->value_len;
                    xsend(resp);
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


