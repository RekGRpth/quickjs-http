#include <arpa/inet.h>
//#include <errno.h>
#include <netdb.h>
//#include <netinet/in.h>
#include <quickjs/quickjs-libc.h>
//#include <stdio.h>
//#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

static JSValue js_accept(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int sockfd;
    if (JS_ToInt32(ctx, &sockfd, argv[0])) return JS_EXCEPTION;
    int new_sockfd = accept(sockfd, NULL, NULL);
    if (new_sockfd < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_NewInt32(ctx, new_sockfd);
}

static int resolve_host(const char *name_or_ip, int port, struct sockaddr_in *addr4, struct sockaddr_in6 *addr6) {
    port = htons(port); // port in network order
    if (inet_pton(AF_INET, name_or_ip, &addr4->sin_addr)) {
        memset(addr6, 0, sizeof(*addr6));
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
    } else if (inet_pton(AF_INET6, name_or_ip, &addr6->sin6_addr)) {
        memset(addr4, 0, sizeof(*addr4));
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
    } else {
        struct addrinfo hints, *ret;
        memset(&hints, 0, sizeof(hints));
        memset(addr4, 0, sizeof(*addr4));
        memset(addr6, 0, sizeof(*addr6));
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(name_or_ip, NULL, &hints, &ret)) return -1;
        for (struct addrinfo *cur = ret; cur; cur = cur->ai_next) {
            if (cur->ai_addr->sa_family == AF_INET) {
                memcpy(addr4, cur->ai_addr, sizeof(*addr4));
                addr4->sin_port = port;
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
    int sockfd, rc, af = 0;
    if (JS_ToInt32(ctx, &sockfd, argv[0])) return JS_EXCEPTION;
    int port;
    if (JS_ToInt32(ctx, &port, argv[2])) return JS_EXCEPTION;
    const char *host = JS_ToCString(ctx, argv[1]);
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
    if (!resolve_host(host, port, &addr4, &addr6)) {
        if (addr4.sin_family == AF_INET) af = AF_INET;
        else if (addr6.sin6_family == AF_INET6) af = AF_INET6;
    }
    JS_FreeCString(ctx, host);
    switch (af) {
        case AF_INET: rc = bind(sockfd, (struct sockaddr *)&addr4, sizeof(addr4)); break;
        case AF_INET6: rc = bind(sockfd, (struct sockaddr *)&addr6, sizeof(addr6)); break;
        default: return JS_ThrowInternalError(ctx, "Not valid hostname or IP address");
    }
    if (rc < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_UNDEFINED;
}

static JSValue js_listen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int sockfd;
    if (JS_ToInt32(ctx, &sockfd, argv[0])) return JS_EXCEPTION;
    int backlog;
    if (JS_ToInt32(ctx, &backlog, argv[1])) return JS_EXCEPTION;
    int rc = listen(sockfd, backlog);
    if (rc < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_UNDEFINED;
}

static JSValue js_setsockopt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    int sockfd;
    if (JS_ToInt32(ctx, &sockfd, argv[0])) return JS_EXCEPTION;
    int level;
    if (JS_ToInt32(ctx, &level, argv[1])) return JS_EXCEPTION;
    int option;
    if (JS_ToInt32(ctx, &option, argv[2])) return JS_EXCEPTION;
    int value;
    if (JS_ToInt32(ctx, &value, argv[3])) return JS_EXCEPTION;
    int rc = setsockopt(sockfd, level, option, &value, sizeof(value));
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
    int sockfd = socket(domain, type, protocol);
//    if (sockfd < 0) return JS_ThrowInternalError(ctx, "%d -> %s", errno, strerror(errno));
    if (sockfd < 0) return JS_ThrowInternalError(ctx, "%m");
    return JS_NewInt32(ctx, sockfd);
}

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static const JSCFunctionListEntry js_http_funcs[] = {
    JS_CFUNC_DEF("accept", 1, js_accept),
    JS_CFUNC_DEF("bind", 3, js_bind),
    JS_CFUNC_DEF("listen", 2, js_listen),
    JS_CFUNC_DEF("setsockopt", 4, js_setsockopt),
    JS_CFUNC_DEF("socket", 3, js_socket),
#define DEF(x) JS_PROP_INT32_DEF(#x, x, JS_PROP_CONFIGURABLE )
    DEF(AF_INET),
    DEF(SOCK_STREAM),
    DEF(SOL_SOCKET),
    DEF(SOMAXCONN),
    DEF(SO_REUSEADDR),
    DEF(SO_REUSEPORT),
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
