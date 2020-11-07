#include <arpa/inet.h>
//#include <errno.h>
//#include <linux/limits.h>
#include <netdb.h>
//#include <netinet/in.h>
#include <netinet/tcp.h>
//#include <pthread.h>
#include <quickjs/quickjs-libc.h>
//#include <spawn.h>
//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
//#include <sys/wait.h>
#include <unistd.h>

static JSValue js_sockaddr_to_value(JSContext *ctx, const struct sockaddr *addr) {
    JSValue value = JS_NewObject(ctx);
    if (JS_IsException(value)) return value;
    switch (addr->sa_family) {
        case AF_INET: {
            struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
            char buf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr4->sin_addr, (char *__restrict)&buf, INET_ADDRSTRLEN)) JS_DefinePropertyValueStr(ctx, value, "ip", JS_NewString(ctx, buf), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, value, "port", JS_NewInt32(ctx, ntohs(addr4->sin_port)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, value, "family", JS_NewInt32(ctx, AF_INET), JS_PROP_C_W_E);
        } break;
        case AF_INET6: {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;
            char buf[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &addr6->sin6_addr, (char *__restrict)&buf, INET6_ADDRSTRLEN)) JS_DefinePropertyValueStr(ctx, value, "ip", JS_NewString(ctx, buf), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, value, "port", JS_NewInt32(ctx, ntohs(addr6->sin6_port)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, value, "family", JS_NewInt32(ctx, AF_INET6), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, value, "flowinfo", JS_NewInt32(ctx, ntohl(addr6->sin6_flowinfo)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, value, "scope_id", JS_NewInt32(ctx, addr6->sin6_scope_id), JS_PROP_C_W_E);
        } break;
        default: return JS_ThrowInternalError(ctx, "sa_family = %i", addr->sa_family);
    }
    return value;
}

static JSValue js_accept(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    struct sockaddr_storage addr;
    socklen_t addrlen;
    int new_fd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    if (new_fd < 0) return JS_ThrowInternalError(ctx, "accept(%i): %m", fd);
    int val = 0;
    if (setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val)) < 0) return JS_ThrowInternalError(ctx, "setsockopt(%i, %i, %i, %i): %m", new_fd, IPPROTO_TCP, TCP_NODELAY, val);
    if (setsockopt(new_fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) < 0) return JS_ThrowInternalError(ctx, "setsockopt(%i, %i, %i, %i): %m", new_fd, IPPROTO_TCP, TCP_NODELAY, val);
    JSValue value = js_sockaddr_to_value(ctx, (struct sockaddr *)&addr);
    if (JS_IsException(value)) return value;
    JS_DefinePropertyValueStr(ctx, value, "fd", JS_NewInt32(ctx, new_fd), JS_PROP_C_W_E);
    return value;
}

static JSValue js_getpeername(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    struct sockaddr addr;
    socklen_t addrlen;
    int rc = getpeername(fd, &addr, &addrlen);
    if (rc < 0) return JS_ThrowInternalError(ctx, "getpeername(%i), %m", fd);
    return js_sockaddr_to_value(ctx, &addr);
}

static JSValue js_getsockname(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    struct sockaddr addr;
    socklen_t addrlen;
    int rc = getsockname(fd, &addr, &addrlen);
    if (rc < 0) return JS_ThrowInternalError(ctx, "getsockname(%i), %m", fd);
    return js_sockaddr_to_value(ctx, &addr);
}

static JSValue js_listen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue value = JS_EXCEPTION;
    const char *node = JS_ToCString(ctx, argv[0]);
    if (!node) goto ret;
    const char *service = JS_ToCString(ctx, argv[1]);
    if (!service) goto free_node;
    int backlog;
    if (JS_ToInt32(ctx, &backlog, argv[2])) goto free_service;
    const struct addrinfo hints = {
        .ai_flags = AI_PASSIVE,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
        .ai_addrlen = 0,
        .ai_addr = NULL,
        .ai_canonname = NULL,
        .ai_next = NULL
    };
    struct addrinfo *ret, *rp = NULL;
    if (getaddrinfo(node, service, &hints, &ret) < 0) { value = JS_ThrowInternalError(ctx, "getaddrinfo: %m"); goto free_service; }
    int fd = -1;
    for (rp = ret; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        int value = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0) goto close_fd;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value)) < 0) goto close_fd;
        if (!bind(fd, rp->ai_addr, rp->ai_addrlen)) break;
close_fd:
        close(fd);
    }
    if (fd < 0 || !rp) { value = JS_ThrowInternalError(ctx, "bind: %m"); goto free_service; }
    if (listen(fd, backlog) < 0) { value = JS_ThrowInternalError(ctx, "listen: %m"); goto free_service; }
    value = js_sockaddr_to_value(ctx, rp->ai_addr);
    if (JS_IsException(value)) goto free_service;
    JS_DefinePropertyValueStr(ctx, value, "fd", JS_NewInt32(ctx, fd), JS_PROP_C_W_E);
    freeaddrinfo(ret);
free_service:
    JS_FreeCString(ctx, service);
free_node:
    JS_FreeCString(ctx, node);
ret:
    return value;
}

static JSValue js_loop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_std_loop(ctx);
    return JS_UNDEFINED;
}

static JSValue js_recv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    int size;
    if (JS_ToInt32(ctx, &size, argv[1])) return JS_EXCEPTION;
    int flags;
    if (JS_ToInt32(ctx, &flags, argv[2])) return JS_EXCEPTION;
    char buf[size];
    ssize_t len = recv(fd, buf, size, flags);
    if (len < 0) return JS_ThrowInternalError(ctx, "recv(%i): %m", fd);
    return JS_NewStringLen(ctx, buf, len);
}

static JSValue js_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    int flags;
    if (JS_ToInt32(ctx, &flags, argv[2])) return JS_EXCEPTION;
    size_t len;
    const char *buf = JS_ToCStringLen(ctx, &len, argv[1]);
    if (!buf) return JS_EXCEPTION;
    ssize_t rc = send(fd, buf, len, flags);
    if (rc < 0) return JS_ThrowInternalError(ctx, "send(%i): %m", fd);
    JS_FreeCString(ctx, buf);
    return JS_NewInt32(ctx, rc);
}

static JSValue js_setsockopt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    int level;
    if (JS_ToInt32(ctx, &level, argv[1])) return JS_EXCEPTION;
    int option;
    if (JS_ToInt32(ctx, &option, argv[2])) return JS_EXCEPTION;
    int value;
    if (JS_ToInt32(ctx, &value, argv[3])) return JS_EXCEPTION;
    int rc = setsockopt(fd, level, option, &value, sizeof(value));
    if (rc < 0) return JS_ThrowInternalError(ctx, "setsockopt(%i, %i, %i, %i): %m", fd, level, option, value);
    return JS_UNDEFINED;
}

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static const JSCFunctionListEntry js_http_funcs[] = {
    JS_CFUNC_DEF("accept", 1, js_accept),
    JS_CFUNC_DEF("getpeername", 1, js_getpeername),
    JS_CFUNC_DEF("getsockname", 1, js_getsockname),
    JS_CFUNC_DEF("listen", 2, js_listen),
    JS_CFUNC_DEF("loop", 0, js_loop),
    JS_CFUNC_DEF("recv", 3, js_recv),
    JS_CFUNC_DEF("send", 3, js_send),
#define DEF(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE )
    DEF(SOMAXCONN),
};

static int js_http_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, js_http_funcs, countof(js_http_funcs));
}

#ifdef JS_SHARED_LIBRARY
#define JS_INIT_MODULE js_init_module
#else
#define JS_INIT_MODULE js_init_module_http
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name) {
    JSModuleDef *m = JS_NewCModule(ctx, module_name, js_http_init);
    if (!m) return NULL;
    JS_AddModuleExportList(ctx, m, js_http_funcs, countof(js_http_funcs));
    return m;
}
