#include <arpa/inet.h>
#include <errno.h>
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

static JSValue js_accept(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd, new_fd = -1;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    struct sockaddr addr;
    socklen_t addrlen;
    int rc = getsockname(fd, &addr, &addrlen);
    if (rc < 0) return JS_ThrowInternalError(ctx, "%m");
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    switch (addr.sa_family) {
        case AF_INET: {
            socklen_t addr4len;
            new_fd = accept(fd, (struct sockaddr *)&addr4, &addr4len);
        } break;
        case AF_INET6: {
            socklen_t addr6len;
            new_fd = accept(fd, (struct sockaddr *)&addr6, &addr6len);
        } break;
        default: return JS_UNDEFINED;
    }
    if (new_fd < 0) return JS_ThrowInternalError(ctx, "%m");
    JSValue obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) return obj;
    JS_DefinePropertyValueStr(ctx, obj, "fd", JS_NewInt32(ctx, new_fd), JS_PROP_C_W_E);
    switch (addr.sa_family) {
        case AF_INET: {
            char buf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr4.sin_addr, (char *__restrict)&buf, INET_ADDRSTRLEN)) JS_DefinePropertyValueStr(ctx, obj, "host", JS_NewString(ctx, buf), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "port", JS_NewInt32(ctx, ntohs(addr4.sin_port)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "family", JS_NewInt32(ctx, AF_INET), JS_PROP_C_W_E);
        } break;
        case AF_INET6: {
            char buf[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &addr6.sin6_addr, (char *__restrict)&buf, INET6_ADDRSTRLEN)) JS_DefinePropertyValueStr(ctx, obj, "host", JS_NewString(ctx, buf), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "port", JS_NewInt32(ctx, ntohs(addr6.sin6_port)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "family", JS_NewInt32(ctx, AF_INET6), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "flowinfo", JS_NewInt32(ctx, ntohl(addr6.sin6_flowinfo)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "scope_id", JS_NewInt32(ctx, addr6.sin6_scope_id), JS_PROP_C_W_E);
        } break;
    }
    return obj;
}

static int resolve_host(const char *name_or_ip, int port, struct sockaddr_in *addr, struct sockaddr_in6 *addr6) {
    port = htons(port); // port in network order
    if (inet_pton(AF_INET, name_or_ip, &addr->sin_addr)) {
        memset(addr6, 0, sizeof(*addr6));
        addr->sin_family = AF_INET;
        addr->sin_port = port;
    } else if (inet_pton(AF_INET6, name_or_ip, &addr6->sin6_addr)) {
        memset(addr, 0, sizeof(*addr));
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
    } else {
        struct addrinfo hints, *ret;
        memset(&hints, 0, sizeof(hints));
        memset(addr, 0, sizeof(*addr));
        memset(addr6, 0, sizeof(*addr6));
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(name_or_ip, NULL, &hints, &ret)) return -1;
        for (struct addrinfo *cur = ret; cur; cur = cur->ai_next) {
            if (cur->ai_addr->sa_family == AF_INET) {
                memcpy(addr, cur->ai_addr, sizeof(*addr));
                addr->sin_port = port;
            } else if (cur->ai_addr->sa_family == AF_INET6) {
                memcpy(addr6, cur->ai_addr, sizeof(*addr6));
                addr6->sin6_port = port;
            }
        }
        freeaddrinfo(ret);
    }
    return 0;
}

static JSValue js_bind(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd, rc, af = 0;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    int port;
    if (JS_ToInt32(ctx, &port, argv[2])) return JS_EXCEPTION;
    const char *host = JS_ToCString(ctx, argv[1]);
    if (!host) return JS_EXCEPTION;
    struct sockaddr_in addr;
    struct sockaddr_in6 addr6;
    if (!resolve_host(host, port, &addr, &addr6)) {
        if (addr.sin_family == AF_INET) af = AF_INET;
        else if (addr6.sin6_family == AF_INET6) af = AF_INET6;
    }
    JS_FreeCString(ctx, host);
    switch (af) {
        case AF_INET: rc = bind(fd, (struct sockaddr *)&addr, sizeof(addr)); break;
        case AF_INET6: rc = bind(fd, (struct sockaddr *)&addr6, sizeof(addr6)); break;
        default: return JS_ThrowInternalError(ctx, "Not valid hostname or IP address");
    }
    if (rc < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_NewInt32(ctx, af);
}

static JSValue js_getsockname(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    struct sockaddr addr;
    socklen_t addrlen;
    int rc = getsockname(fd, &addr, &addrlen);
    if (rc < 0) return JS_ThrowInternalError(ctx, "%m");
    JSValue obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) return obj;
    switch (addr.sa_family) {
        case AF_INET: {
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&addr;
            char buf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &addr4->sin_addr, (char *__restrict)&buf, INET_ADDRSTRLEN)) JS_DefinePropertyValueStr(ctx, obj, "host", JS_NewString(ctx, buf), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "port", JS_NewInt32(ctx, ntohs(addr4->sin_port)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "family", JS_NewInt32(ctx, AF_INET), JS_PROP_C_W_E);
        } break;
        case AF_INET6: {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&addr;
            char buf[INET6_ADDRSTRLEN];
            if (inet_ntop(AF_INET6, &addr6->sin6_addr, (char *__restrict)&buf, INET6_ADDRSTRLEN)) JS_DefinePropertyValueStr(ctx, obj, "host", JS_NewString(ctx, buf), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "port", JS_NewInt32(ctx, ntohs(addr6->sin6_port)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "family", JS_NewInt32(ctx, AF_INET6), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "flowinfo", JS_NewInt32(ctx, ntohl(addr6->sin6_flowinfo)), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, obj, "scope_id", JS_NewInt32(ctx, addr6->sin6_scope_id), JS_PROP_C_W_E);
        } break;
        default: return JS_UNDEFINED;
    }
    return obj;
}

static JSValue js_listen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) return JS_EXCEPTION;
    int backlog;
    if (JS_ToInt32(ctx, &backlog, argv[1])) return JS_EXCEPTION;
    int rc = listen(fd, backlog);
    if (rc < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_UNDEFINED;
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
    if (len < 0) {
        if (errno == ECONNRESET) return JS_NULL;
        return JS_ThrowInternalError(ctx, "%m");
    }
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
    if (rc < 0) {
        if (errno == ENOTSOCK) return JS_NULL;
        return JS_ThrowInternalError(ctx, "%m");
    }
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
    if (rc < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_UNDEFINED;
}

static JSValue js_socket(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int domain;
    if (JS_ToInt32(ctx, &domain, argv[0])) return JS_EXCEPTION;
    int type;
    if (JS_ToInt32(ctx, &type, argv[1])) return JS_EXCEPTION;
    int protocol;
    if (JS_ToInt32(ctx, &protocol, argv[2])) return JS_EXCEPTION;
    int fd = socket(domain, type, protocol);
    if (fd < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_NewInt32(ctx, fd);
}

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static const JSCFunctionListEntry js_http_funcs[] = {
    JS_CFUNC_DEF("accept", 1, js_accept),
    JS_CFUNC_DEF("bind", 3, js_bind),
    JS_CFUNC_DEF("getsockname", 1, js_getsockname),
    JS_CFUNC_DEF("listen", 2, js_listen),
    JS_CFUNC_DEF("loop", 0, js_loop),
    JS_CFUNC_DEF("recv", 3, js_recv),
    JS_CFUNC_DEF("send", 3, js_send),
    JS_CFUNC_DEF("setsockopt", 4, js_setsockopt),
    JS_CFUNC_DEF("socket", 3, js_socket),
#define DEF(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE )
    DEF(AF_INET),
    DEF(AF_INET6),
    DEF(AF_UNSPEC),
    DEF(IPPROTO_TCP),
    DEF(SOCK_STREAM),
    DEF(SO_KEEPALIVE),
    DEF(SOL_SOCKET),
    DEF(SOMAXCONN),
    DEF(SO_REUSEADDR),
    DEF(SO_REUSEPORT),
    DEF(STDERR_FILENO),
    DEF(STDIN_FILENO),
    DEF(STDOUT_FILENO),
    DEF(TCP_NODELAY),
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
