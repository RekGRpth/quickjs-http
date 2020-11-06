//seq $(nproc) | xargs -r -n 1 -P "$(nproc)" qjs http.js
import * as http from "http.so"
import * as os from "os"
import * as std from "std"
const server = {
    host: '0.0.0.0',
    port: 8080,
    text: 'Hello, World!',
    END: '\r\n\r\n',
    update: () => {
        server.time = (new Date()).toUTCString()
        server.rTEXT = `HTTP/1.1 200 OK\r\nServer: j\r\nDate: ${server.time}\r\nContent-Type: text/plain\r\nContent-Length: `
        os.setTimeout(server.update, 1000)
    },
}
server.fd = http.socket(http.AF_INET, http.SOCK_STREAM | http.SOCK_NONBLOCK, 0)
http.setsockopt(server.fd, http.SOL_SOCKET, http.SO_REUSEADDR, 1)
http.setsockopt(server.fd, http.SOL_SOCKET, http.SO_REUSEPORT, 1)
server.af = http.bind(server.fd, server.host, server.port)
//console.log(JSON.stringify(http))
http.listen(server.fd, http.SOMAXCONN)
server.update()
console.log(JSON.stringify(server))
os.setReadHandler(server.fd, () => {
    const client = http.accept(server.fd)
    console.log(JSON.stringify({server: server, client: client}))
    http.setsockopt(client.fd, http.IPPROTO_TCP, http.TCP_NODELAY, 0)
    http.setsockopt(client.fd, http.SOL_SOCKET, http.SO_KEEPALIVE, 0)
    os.setReadHandler(client.fd, () => {
        client.request = http.recv(client.fd, 128, 0)
        if (client.request && client.request.length) {
            client.response = `${server.rTEXT}${server.text.length}${server.END}${server.text}`
            console.log(JSON.stringify({server: server, client: client}))
            os.setWriteHandler(client.fd, () => {
                client.request = undefined
                http.send(client.fd, client.response, http.MSG_NOSIGNAL)
                os.setWriteHandler(client.fd, null)
                client.response = undefined
            })
        } else {
            os.setReadHandler(client.fd, null)
            os.close(client.fd)
            client.fd = undefined
        }
    })
})
http.loop()
