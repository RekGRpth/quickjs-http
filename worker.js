import * as http from "http.so"
import * as os from "os"
import * as std from "std"
let time
function update() {
    time = (new Date()).toUTCString()
    os.setTimeout(update, 1000)
}
update()
for (let i = 0; i < 8; i++) new os.Worker("http.js")
http.loop()
