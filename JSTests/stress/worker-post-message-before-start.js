function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

asyncTestStart(1);

// Nothing waits for the worker thread to come up, so these messages are posted before the
// worker has a port to receive them on. The channel has to hold them, in order, until it does.
let worker = new Worker(`
    let seen = [];
    onmessage = function (event) {
        seen.push(event.data);
        if (seen.length < 3)
            return;
        postMessage(seen.join(','));
        close();
    };
`, { source: "string" });

worker.onmessage = function (event) {
    shouldBe(event.data, 'a,b,c');
    asyncTestPassed();
};

worker.postMessage('a');
worker.postMessage('b');
worker.postMessage('c');
