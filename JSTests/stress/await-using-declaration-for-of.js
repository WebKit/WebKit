//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

async function testForOf() {
    var order = [];
    var resources = [
        { id: "a", [Symbol.asyncDispose]() { order.push("dispose-a"); } },
        { id: "b", [Symbol.asyncDispose]() { order.push("dispose-b"); } },
        { id: "c", [Symbol.asyncDispose]() { order.push("dispose-c"); } },
    ];
    for (await using r of resources) {
        order.push("body-" + r.id);
    }
    shouldBe(order.join(","), "body-a,dispose-a,body-b,dispose-b,body-c,dispose-c");
}

async function testForAwaitOf() {
    var order = [];
    var asyncIterable = {
        [Symbol.asyncIterator]() {
            var i = 0;
            return {
                next() {
                    if (i < 2) {
                        var id = String.fromCharCode(97 + i);
                        i++;
                        return Promise.resolve({ value: { id, [Symbol.asyncDispose]() { order.push("dispose-" + id); } }, done: false });
                    }
                    return Promise.resolve({ value: undefined, done: true });
                }
            };
        }
    };
    for await (await using r of asyncIterable) {
        order.push("body-" + r.id);
    }
    shouldBe(order.join(","), "body-a,dispose-a,body-b,dispose-b");
}

async function testForOfContinue() {
    var order = [];
    var resources = [
        { id: "a", [Symbol.asyncDispose]() { order.push("dispose-a"); } },
        { id: "b", [Symbol.asyncDispose]() { order.push("dispose-b"); } },
    ];
    for (await using r of resources) {
        order.push("pre-" + r.id);
        if (r.id === "a") continue;
        order.push("post-" + r.id);
    }
    shouldBe(order.join(","), "pre-a,dispose-a,pre-b,post-b,dispose-b");
}

async function testForOfBreak() {
    var order = [];
    var resources = [
        { id: "a", [Symbol.asyncDispose]() { order.push("dispose-a"); } },
        { id: "b", [Symbol.asyncDispose]() { order.push("dispose-b"); } },
    ];
    for (await using r of resources) {
        order.push("body-" + r.id);
        if (r.id === "a") break;
    }
    shouldBe(order.join(","), "body-a,dispose-a");
}

async function main() {
    await testForOf();
    await testForAwaitOf();
    await testForOfContinue();
    await testForOfBreak();
}

main().then(() => {}, e => { print("FAIL: " + e); $vm.abort(); });
drainMicrotasks();
