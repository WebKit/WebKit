function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

asyncTestStart(1);

let worker = new Worker(`
    self.onmessage = function (event) {
        let { number, string, map } = event.data;
        postMessage({ number: number * 2, string: string + "!", map: new Map([...map, ["fromWorker", true]]), isSelfGlobal: self === globalThis });
        close();
    };
`, { source: "string" });

shouldBe(worker instanceof Worker, true);
shouldBe(Object.getPrototypeOf(worker), Worker.prototype);

{
    let badPrototype = function () { };
    badPrototype.prototype = 1;
    let fallback = Reflect.construct(Worker, ["", { source: "string" }], badPrototype);
    shouldBe(Object.getPrototypeOf(fallback), Worker.prototype);
    shouldBe(typeof fallback.postMessage, "function");
    fallback.terminate();

    class Subclass extends Worker { }
    let subclassed = new Subclass("", { source: "string" });
    shouldBe(Object.getPrototypeOf(subclassed), Subclass.prototype);
    shouldBe(subclassed instanceof Worker, true);
    subclassed.terminate();
}

worker.onmessage = function (event) {
    shouldBe(this, worker);
    shouldBe(event.data.number, 42);
    shouldBe(event.data.string, "hello!");
    shouldBe(event.data.map.get("fromParent"), 1);
    shouldBe(event.data.map.get("fromWorker"), true);
    shouldBe(event.data.isSelfGlobal, true);
    asyncTestPassed();
};

worker.postMessage({ number: 21, string: "hello", map: new Map([["fromParent", 1]]) });
