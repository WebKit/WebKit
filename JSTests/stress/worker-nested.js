function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

asyncTestStart(1);

// Nested workers are unsupported, so a worker scope has no Worker constructor at all.
// Supporting them needs a parent-side registry of the channels a thread owns, terminated
// before its VM is torn down, the way WebCore stops a scope's ActiveDOMObjects.
let worker = new Worker(`
    postMessage(typeof Worker);
`, { source: "string" });

worker.onmessage = function (event) {
    shouldBe(event.data, "undefined");
    worker.terminate();
    asyncTestPassed();
};
