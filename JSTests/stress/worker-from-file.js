function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

asyncTestStart(1);

let worker = new Worker("./resources/worker-file-resource.js");
worker.onmessage = function (event) {
    shouldBe(event.data, "file worker got ping");
    asyncTestPassed();
};
worker.postMessage("ping");
