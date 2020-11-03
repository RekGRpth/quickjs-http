import * as http from "libhttp.so"
import * as os from "os"
import * as std from "std"
const server = { host: '0.0.0.0', port: 8080 }
server.sockfd = http.socket(http.AF_INET, http.SOCK_STREAM | http.SOCK_NONBLOCK, 0)
http.setsockopt(server.sockfd, http.SOL_SOCKET, http.SO_REUSEADDR, 1)
http.setsockopt(server.sockfd, http.SOL_SOCKET, http.SO_REUSEPORT, 1)
server.af = http.bind(server.sockfd, server.host, server.port)
http.listen(server.sockfd, http.SOMAXCONN)
let time, rTEXT
function update() {
    time = (new Date()).toUTCString()
    rTEXT = `HTTP/1.1 200 OK\r\nServer: j\r\nDate: ${time}\r\nContent-Type: text/plain\r\nContent-Length: `
    os.setTimeout(update, 100)
}
const text = 'Hello, World!'
const END = '\r\n\r\n'
update()
while (true) {
    const client = http.accept(server.sockfd, server.af)
    http.setsockopt(client.sockfd, http.IPPROTO_TCP, http.TCP_NODELAY, 0)
    http.setsockopt(client.sockfd, http.SOL_SOCKET, http.SO_KEEPALIVE, 0)
    console.log(JSON.stringify({fd: client.sockfd, host: client.host, port: client.port}))
    http.send(client.sockfd, `${rTEXT}${text.length}${END}${text}`, http.MSG_NOSIGNAL)
    os.close(client.sockfd)
    http.loop()
}
