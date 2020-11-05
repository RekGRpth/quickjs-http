import * as http from "libhttp.so"
import * as os from "os"
import * as std from "std"
const server = { host: '0.0.0.0', port: 8080 }
server.fd = http.socket(http.AF_INET, http.SOCK_STREAM | http.SOCK_NONBLOCK, 0)
http.setsockopt(server.fd, http.SOL_SOCKET, http.SO_REUSEADDR, 1)
http.setsockopt(server.fd, http.SOL_SOCKET, http.SO_REUSEPORT, 1)
server.af = http.bind(server.fd, server.host, server.port)
http.listen(server.fd, http.SOMAXCONN)
console.log(JSON.stringify(server))
let time, rTEXT
function update() {
    time = (new Date()).toUTCString()
    rTEXT = `HTTP/1.1 200 OK\r\nServer: j\r\nDate: ${time}\r\nContent-Type: text/plain\r\nContent-Length: `
//    os.setTimeout(update, 100)
}
const text = 'Hello, World!'
const END = '\r\n\r\n'
update()
while (true) {
    const client = http.accept(server.fd, server.af)
    console.log(JSON.stringify(client))
    http.setsockopt(client.fd, http.IPPROTO_TCP, http.TCP_NODELAY, 0)
    http.setsockopt(client.fd, http.SOL_SOCKET, http.SO_KEEPALIVE, 0)
    while (true) {
        const request = http.recv(client.fd, 0)
        if (!request || !request.length) break
//        console.log(JSON.stringify({request: request}))
        http.send(client.fd, `${rTEXT}${text.length}${END}${text}`, http.MSG_NOSIGNAL)
    }
    os.close(client.fd)
    http.loop()
}
