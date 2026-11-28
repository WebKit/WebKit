function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

asyncTestStart(3);

let fileWorker = new Worker("./resources/worker-module-resource.mjs", { type: "module" });
shouldBe(fileWorker.onmessage, null);
fileWorker.onmessage = (event) => {
    shouldBe(event.data.text, "module worker got ping");
    shouldBe(event.data.thisIsUndefined, true);
    asyncTestPassed();
};
fileWorker.postMessage("ping");

let inlineWorker = new Worker(`
    const { value } = await Promise.resolve({ value: 41 });
    onmessage = (event) => {
        postMessage(event.data + value);
        close();
    };
`, { type: "module", source: "string" });
inlineWorker.onmessage = (event) => {
    shouldBe(event.data, 42);
    asyncTestPassed();
};
inlineWorker.postMessage(1);

let strictClassicWorker = new Worker(`
    "use strict";
    if (onmessage !== null)
        throw new Error("onmessage should start out null");
    onmessage = (event) => {
        postMessage(event.data);
        close();
    };
`, { source: "string" });
strictClassicWorker.onmessage = (event) => {
    shouldBe(event.data, "strict");
    asyncTestPassed();
};
strictClassicWorker.postMessage("strict");
