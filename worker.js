import * as os from "os"
import * as std from "std"
for (let i = 0; i < 8; i++) {
    new os.Worker("http.js")
}
