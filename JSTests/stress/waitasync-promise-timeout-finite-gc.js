var sab = new SharedArrayBuffer(3 * 4);
var i32a = new Int32Array(sab);

var numWorkers = 0;
function startWorker(code) {
    numWorkers++;
    $.agent.start(code);
}

const WAIT_INDEX = 0;
const TOTAL_WAITER_COUNT = 1;

startWorker(`
    $.agent.receiveBroadcast((sab) => {

        (function() {
            var i32a = new Int32Array(sab);
    
            Atomics.waitAsync(i32a, ${WAIT_INDEX}, 0, 100).value.then((value) => {
                $.agent.report("value " + value);
                $.agent.report("done");
            },
            () => {
                $.agent.report("error");
            });
        })();

        gc();
    });
`);

$.agent.broadcast(sab);

let waiters = 0;
do {
    waiters = waiterListSize(i32a, WAIT_INDEX);
} while (waiters != TOTAL_WAITER_COUNT);

if (Atomics.notify(i32a, WAIT_INDEX, 1) != TOTAL_WAITER_COUNT)
    throw new Error(`Atomics.notify should return ${TOTAL_WAITER_COUNT}.`);

for (; ;) {
    var report = waitForReport();
    if (report == "done") {
        if (!--numWorkers) {
            print("All workers done!");
            break;
        }
    } else if (report.startsWith('value')) {
        if (report != 'value ok')
            throw new Error("The promise should be resolved with ok");
    } else
        print("report: " + report);
}

if (waiterListSize(i32a, WAIT_INDEX) != 0)
    throw new Error("The WaiterList should be empty.");
