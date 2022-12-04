var sab = new SharedArrayBuffer(3 * 4);
var i32a = new Int32Array(sab);

var numWorkers = 0;
function startWorker(code) {
    numWorkers++;
    $.agent.start(code);
}

const WAIT_INDEX = 0;
const READY_INDEX_A = 1;
const READY_INDEX_B = 2;
const NOTIFY_COUNT = 2;

startWorker(`
    $.agent.receiveBroadcast(async (sab) => {
        var i32a = new Int32Array(sab);
    
        let p = Atomics.waitAsync(i32a, ${WAIT_INDEX}, 0, undefined).value;

        Atomics.store(i32a, ${READY_INDEX_A}, 1);
        Atomics.notify(i32a, ${READY_INDEX_A});

        let res = await p;
        if (res !== 'ok')
            throw new Error("A resolve: " + res);

        $.agent.report("done");
    });
`);

startWorker(`
    $.agent.receiveBroadcast(async (sab) => {
        var i32a = new Int32Array(sab);
    
        let p = Atomics.waitAsync(i32a, ${WAIT_INDEX}, 0, undefined).value;

        Atomics.store(i32a, ${READY_INDEX_B}, 1);
        Atomics.notify(i32a, ${READY_INDEX_B});

        let res = await p;
        if (res !== 'ok')
            throw new Error("B resolve: " + res);

        $.agent.report("done");
    });
`);

startWorker(`
    $.agent.receiveBroadcast((sab) => {
        var i32a = new Int32Array(sab);
    
        Atomics.wait(i32a, ${READY_INDEX_A}, 0);
        Atomics.wait(i32a, ${READY_INDEX_B}, 0);

        let res = Atomics.notify(i32a, ${WAIT_INDEX}, ${NOTIFY_COUNT});
        if (res !== 2)
            throw new Error("C notified workers: " + res);

        $.agent.report("done");
    });
`);

$.agent.broadcast(sab);

for (; ;) {
    var report = waitForReport();
    if (report == "done") {
        if (!--numWorkers) {
            // print("All workers done!");
            break;
        }
    } else
        print("report: " + report);
}
