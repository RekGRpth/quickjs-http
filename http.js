import * as http from "libhttp.so"
import * as os from "os"
import * as std from "std"

const server = { host: '0.0.0.0', port: 8080 }
server.sockfd = http.socket(http.AF_INET, http.SOCK_STREAM | http.SOCK_NONBLOCK, 0)
http.setsockopt(server.sockfd, http.SOL_SOCKET, http.SO_REUSEADDR, 1)
http.setsockopt(server.sockfd, http.SOL_SOCKET, http.SO_REUSEPORT, 1)
server.af = http.bind(server.sockfd, server.host, server.port)
http.listen(server.sockfd, http.SOMAXCONN)
while (true) {
    let [fd, address, port] = http.accept(server.sockfd, server.af)
    console.log(JSON.stringify({fd:fd, address: address, port: port}))
    os.close(fd)
    http.loop()
}
