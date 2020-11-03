import * as http from "libhttp.so";

const server = {
    host: '0.0.0.0',
    port: 8080,
    sockfd: http.socket(http.AF_INET, http.SOCK_STREAM | http.SOCK_NONBLOCK, 0),
}
http.setsockopt(server.sockfd, http.SOL_SOCKET, http.SO_REUSEADDR, 1)
http.setsockopt(server.sockfd, http.SOL_SOCKET, http.SO_REUSEPORT, 1)
http.bind(server.sockfd, server.host, server.port)
http.listen(server.sockfd, http.SOMAXCONN)
while (true) {
    let fd = http.accept(server.sockfd)
}
