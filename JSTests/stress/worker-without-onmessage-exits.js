function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

asyncTestStart(1);

// A worker that never installs onmessage can never receive anything, so it must not hold its
// own run loop or its parent's open. Its script still runs to completion first: if the worker
// were torn down early the message below would never arrive, and if it were kept alive the
// shell would hang instead of exiting.
let worker = new Worker(`postMessage("ran");`, { source: "string" });

worker.onmessage = function (event) {
    shouldBe(event.data, "ran");
    asyncTestPassed();
};
