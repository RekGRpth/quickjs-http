import * as http from "libhttp.so"
import * as os from "os"
import * as std from "std"
var worker = []
for (let i = 0; i < 8; i++) worker.push(http.spawnp("http.js"))
