var sab = new SharedArrayBuffer(3 * 4);
var i32a = new Int32Array(sab);

var numWorkers = 0;
function startWorker(code) {
    numWorkers++;
    $.agent.start(code);
}

const WAIT_INDEX = 0;
const WAITER_COUNT = 10;
const ASYNC_WAITER_MULTIPLIER = 2;
const ASYNC_WAITER_COUNT = WAITER_COUNT * ASYNC_WAITER_MULTIPLIER;
const TOTAL_WAITER_COUNT = WAITER_COUNT + ASYNC_WAITER_COUNT;

for (let i = 0; i < WAITER_COUNT; i++) {
    startWorker(`
        $.agent.receiveBroadcast((sab) => {
            var i32a = new Int32Array(sab);

            let result = Atomics.wait(i32a, ${WAIT_INDEX}, 0, undefined);
            if (result !== 'ok')
                throw new Error("Atomics.wait result: " + result);

            $.agent.report("done");
        });
    `);

    startWorker(`
        $.agent.receiveBroadcast(async (sab) => {
            var i32a = new Int32Array(sab);

            let promises = [];

            for (let i = 0; i < ${ASYNC_WAITER_MULTIPLIER}; i++)
                promises.push(Atomics.waitAsync(i32a, ${WAIT_INDEX}, 0).value);

            function check(result) {
                if (result !== 'ok')
                    throw new Error("Atomics.waitAsync resolve: " + result);
            }

            for (let i = 0; i < ${ASYNC_WAITER_MULTIPLIER}; i++)
                check(await promises[i]);

            $.agent.report("done");
        });
    `);
}

$.agent.broadcast(sab);

while (waiterListSize(i32a, WAIT_INDEX) != TOTAL_WAITER_COUNT);

let expectedNotified = TOTAL_WAITER_COUNT / 2;
if (Atomics.notify(i32a, WAIT_INDEX, expectedNotified) != expectedNotified)
    throw new Error(`Atomics.notify should return ${expectedNotified}.`);

while (waiterListSize(i32a, WAIT_INDEX) != expectedNotified);

if (Atomics.notify(i32a, WAIT_INDEX, expectedNotified) != expectedNotified)
    throw new Error(`Atomics.notify should return ${expectedNotified}.`);

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

if (waiterListSize(i32a, WAIT_INDEX) != 0)
    throw new Error("The WaiterList should be empty.");
