//seq $(nproc) | xargs -r -n 1 -P "$(nproc)" qjs http.js
import * as http from 'http.so'
import * as os from 'os'
import * as std from 'std'
const setInterval = (fn, delay) => {
    const wrapper = () => {
        fn()
        return os.setTimeout(wrapper, delay)
    }
    os.setTimeout(wrapper, delay)
}
const text = 'Hello, World!'
const END = '\r\n\r\n'
let time
let rTEXT
setInterval(() => {
    time = (new Date()).toUTCString()
    rTEXT = `HTTP/1.1 200 OK\r\nServer: j\r\nDate: ${time}\r\nContent-Type: text/plain\r\nContent-Length: `
}, 1000)
const server = http.listen('0.0.0.0', '8080', http.SOMAXCONN)
console.log(JSON.stringify({time: time, server: server}))
os.setReadHandler(server.fd, () => {
    const client = http.accept(server.fd)
    console.log(JSON.stringify({time: time, server: server, client: client}))
    const timeout = os.setTimeout(() => {
        os.setReadHandler(client.fd, null)
        os.setWriteHandler(client.fd, null)
        os.close(client.fd)
        client.fd = undefined
    }, 70 * 1000)
    os.setReadHandler(client.fd, () => {
        try {
            client.request = http.parse(client.fd, 128, 0)
        } catch (exception) {
            console.log(JSON.stringify({time: time, server: server, client: client, message: exception.message}))
            os.clearTimeout(timeout)
            os.setReadHandler(client.fd, null)
            os.setWriteHandler(client.fd, null)
            os.close(client.fd)
            client.fd = undefined
            return
        }
        if (client.request) {
            [client.request.path, client.request.query] = client.request.url && client.request.url.split('?')
            client.request.vars = {}
            for (let kv of client.request.query.split('&')) {
                let [k, v] = kv.split('=')
                client.request.vars[k] = v === undefined ? null : v && decodeURIComponent(v)
            }
            client.response = `${rTEXT}${text.length}${END}${text}`
            console.log(JSON.stringify({time: time, server: server, client: client}))
            os.setWriteHandler(client.fd, () => {
                client.request = undefined
                http.send(client.fd, client.response, http.MSG_NOSIGNAL)
                os.setWriteHandler(client.fd, null)
                client.response = undefined
            })
        } else {
            os.clearTimeout(timeout)
            os.setReadHandler(client.fd, null)
            os.setWriteHandler(client.fd, null)
            os.close(client.fd)
            client.fd = undefined
        }
    })
})
http.loop()
