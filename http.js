import * as http from "libhttp.so"
import * as os from "os"
import * as std from "std"
let time, rTEXT
function update() {
    time = (new Date()).toUTCString()
    rTEXT = `HTTP/1.1 200 OK\r\nServer: j\r\nDate: ${time}\r\nContent-Type: text/plain\r\nContent-Length: `
    os.setTimeout(update, 1000)
}
const text = 'Hello, World!'
const END = '\r\n\r\n'
update()
const server = { host: '0.0.0.0', port: 8080 }
server.fd = http.socket(http.AF_INET, http.SOCK_STREAM | http.SOCK_NONBLOCK, 0)
http.setsockopt(server.fd, http.SOL_SOCKET, http.SO_REUSEADDR, 1)
http.setsockopt(server.fd, http.SOL_SOCKET, http.SO_REUSEPORT, 1)
server.af = http.bind(server.fd, server.host, server.port)
console.log(JSON.stringify(http))
http.listen(server.fd, http.SOMAXCONN)
console.log(JSON.stringify(server))
os.setReadHandler(server.fd, () => { /*try {*/
    const client = http.accept(server.fd, server.af)
//    console.log(JSON.stringify({time: time, client: client}))
    http.setsockopt(client.fd, http.IPPROTO_TCP, http.TCP_NODELAY, 0)
    http.setsockopt(client.fd, http.SOL_SOCKET, http.SO_KEEPALIVE, 0)
    os.setReadHandler(client.fd, () => { try {
        const request = http.recv(client.fd, 128, 0)
        if (request.length) {
//            console.log(JSON.stringify({time: time, client: client, request: request}))
            const response = `${rTEXT}${text.length}${END}${text}`
//            console.log(JSON.stringify({time: time, client: client, response: response}))
            http.send(client.fd, response, http.MSG_NOSIGNAL)
        } else {
            os.setReadHandler(client.fd, null)
            os.close(client.fd)
        }
    } catch (exception) {
        console.log(JSON.stringify({time: time, client: client, exception: exception.message, stack: exception?.stack}));
//        console.log((e?.stack || "").replace(/^/mg, time));
        os.setReadHandler(client.fd, null)
        os.close(client.fd)
    }})
/*} catch(e) {
    const client_str = JSON.stringify(client)
    console.log(`${client_str} ${time} ${e}`);
//    console.log((e?.stack || "").replace(/^/mg, time));
}*/})
http.loop()
