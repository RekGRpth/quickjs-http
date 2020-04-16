import * as util from "./httputil.so";
import * as os from "os";
import * as std from "std";
import { see } from "./see.js";
export * from "./see.js";
export * from "./httputil.so";

const MAX_REQUEST_SIZE = 20000000;
var _buf = new ArrayBuffer(4);
var _intBuf = new Uint32Array(_buf);
var _workers = {};
var _listenfd, _minWorkers, _maxWorkers, _timeoutSec;
var _workerProcess = 0, _statusWriteFd, _procLimitTimer;

os.signal(os.SIGCHLD, function() {
    let pid;
    while ((pid = os.waitpid(-1, os.WNOHANG)[0]) > 0) {
        //dont try to restart anything here because we could be shutting down
        let workerKey = `w${pid}`;
        if (workerKey in _workers) {
            os.setReadHandler(_workers[workerKey].statusReadFd, null);
            os.close(_workers[workerKey].statusReadFd);
            delete _workers[workerKey];
        }
    }
});

const termSignals = [os.SIGQUIT, os.SIGTERM, os.SIGINT];
termSignals.forEach(s => os.signal(s, function collectWorkers() {
    console.log(`Received signal ${s}`);
    stop();
    std.exit(0);
}));

export function start({ listen, minWorkers = 1, maxWorkers = 10, workerTimeoutSec = 180 }) {
    _listenfd = util.listen(...listen.split(/:/), 10/*backlog*/);
    [_minWorkers, _maxWorkers, _timeoutSec] = [minWorkers, maxWorkers, workerTimeoutSec];
    if (!Number.isInteger(_minWorkers) || !Number.isInteger(_maxWorkers) ||
        !Number.isInteger(workerTimeoutSec) || _maxWorkers < _minWorkers || _minWorkers > 100)
        throw new Error("Invalid config");
    enforceWorkerLimits();
    console.log(`Now serving at http://${listen}`);
    if (_procLimitTimer)
        os.clearTimeout(_procLimitTimer);
    scheduleProcLimiter();
}

function scheduleProcLimiter() {
    _procLimitTimer = os.setTimeout(function() {
        enforceWorkerLimits();
        scheduleProcLimiter();
    }, 2000);
}

export function stop() {
    console.log("Shutting down.");
    for (let workerKey in _workers)
        os.kill(_workers[workerKey].pid, os.SIGINT);
}

function enforceWorkerLimits() { //only parent process should return from here
    let workerKeys = Object.keys(_workers);
    let workerCount = workerKeys.length;
    let idleCount = 0;
    let curMs = new Date().getTime();
    let maxIdleOverMin = Math.max(_minWorkers/3, 3);
    for (let workerKey of workerKeys) { //remove timed out
        let worker = _workers[workerKey];
        if (!worker.idle && (curMs - worker.busySince)/1000 > _timeoutSec) {
            os.kill(worker.pid, os.SIGINT);
            workerCount--;
        }
        if (worker.idle)
            idleCount++;
    }
    while (workerCount < _minWorkers) { //ensure min workers
        newWorker();
        workerCount++;
        idleCount++;
    }
    if (!idleCount && workerCount < _maxWorkers) { //ensure at least one idle
        newWorker();
        workerCount++;
        idleCount++;
    } else if (workerCount > _minWorkers && idleCount > maxIdleOverMin) { //remove extra idles
        for (let workerKey of workerKeys) {
            let worker = _workers[workerKey];
            if (worker.idle && workerCount > _minWorkers && idleCount > maxIdleOverMin) {
                os.kill(worker.pid, os.SIGINT);
                workerCount--;
                idleCount--;
            }
        }
    }
}

function newWorker() {
    if (_workerProcess)
        return;
    let statusReadFd;
    [statusReadFd, _statusWriteFd] = os.pipe();
    let pid = util.fork();

    if (pid < 0)
        throw new Error("Failed to fork a process");
    else if (pid > 0) {
        let workerKey = `w${pid}`;
        os.close(_statusWriteFd);
        _workers[workerKey] = {
            pid: pid,
            statusReadFd: statusReadFd,
            idle: 1,
        };
        os.setReadHandler(statusReadFd, () => {
            let c = os.read(statusReadFd, _buf, 0, 4);
            let worker = _workers[workerKey];
            if (c <= 0 || c != 4 || !worker) {
                os.setReadHandler(statusReadFd, null); //child exited
            } else {
                worker.idle = !_intBuf[0];
                if (!worker.idle)
                    worker.busySince = new Date().getTime();
                //console.log(worker.idle? "idle" : "busy");
            }
        });
    } else { //0=child
        _workerProcess = 1;
        os.close(statusReadFd);
        [os.SIGCHLD, ...termSignals].forEach(s => os.signal(s, null));
        httpWorker();
        std.exit(0); //only parent process should return from here
    }
}

function httpWorker() {
    let connfd, remoteAddr, remotePort;
    while(1) {
        try {
            [connfd, remoteAddr, remotePort] = util.accept(_listenfd);
            signalStatus(1); //busy
            util.readHttpRequest(connfd, MAX_REQUEST_SIZE);
            //console.log(see(util.readHttpRequest(connfd, MAX_REQUEST_SIZE)));
            util.write(connfd, "HTTP/1.1 200\n\nOK");
        } catch(e) {
            console.log(e);
            console.log(e.stack);
        }
        if (connfd)
            os.close(connfd);
        signalStatus(0); //idle
    }

    function signalStatus(s) {
        _intBuf[0] = s? 1 : 0;
        let c = os.write(_statusWriteFd, _buf, 0, 4);
        if (c != 4) {
            std.exit(0); //parent exited
        }
    }
}

