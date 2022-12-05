var sab = new SharedArrayBuffer(3 * 4);
var i32a = new Int32Array(sab);

var numWorkers = 0;
function startWorker(code) {
    numWorkers++;
    $.agent.start(code);
}

const WAIT_INDEX = 0;

startWorker(`
    $.agent.receiveBroadcast((unused_sab) => {
        (function() {
            var sab = new SharedArrayBuffer(8 * 4);
            var i32a = new Int32Array(sab);
    
            Atomics.waitAsync(i32a, ${WAIT_INDEX}, 0, 1).value.then((value) => {
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

for (; ;) {
    var report = waitForReport();
    if (report == "done") {
        if (!--numWorkers) {
            // print("All workers done!");
            break;
        }
    } else if (report.startsWith('value')) {
        if (report != 'value timed-out')
            throw new Error("The promise should be resolved with timed-out");
    } else
        print("report: " + report);
}

if (waiterListSize(i32a, WAIT_INDEX) != 0)
    throw new Error("The WaiterList should be empty.");
