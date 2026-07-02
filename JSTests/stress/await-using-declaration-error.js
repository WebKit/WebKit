//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldThrowAsync(func, errorCheck) {
    let caught = null;
    func().then(() => {}, e => { caught = e; });
    drainMicrotasks();
    if (!caught)
        throw new Error("did not throw");
    if (errorCheck && !errorCheck(caught))
        throw new Error("wrong error: " + caught);
}

shouldThrowAsync(async function() {
    await using a = 42;
}, e => e instanceof TypeError);

shouldThrowAsync(async function() {
    await using a = {};
}, e => e instanceof TypeError);

shouldThrowAsync(async function() {
    await using a = { [Symbol.asyncDispose]: 42 };
}, e => e instanceof TypeError);

async function testDisposeThrows() {
    let caught;
    try {
        await using a = {
            [Symbol.asyncDispose]() { throw new Error("dispose error"); }
        };
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof Error, true);
    shouldBe(caught.message, "dispose error");
}

async function testDisposeRejects() {
    let caught;
    try {
        await using a = {
            [Symbol.asyncDispose]() { return Promise.reject(new Error("reject error")); }
        };
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof Error, true);
    shouldBe(caught.message, "reject error");
}

async function testSuppressedError() {
    let caught;
    try {
        await using a = { [Symbol.asyncDispose]() { throw new Error("a"); } };
        await using b = { [Symbol.asyncDispose]() { throw new Error("b"); } };
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SuppressedError, true);
    shouldBe(caught.error.message, "a");
    shouldBe(caught.suppressed.message, "b");
}

async function testBodyThrowThenDisposeThrow() {
    let caught;
    try {
        await using a = { [Symbol.asyncDispose]() { throw new Error("dispose"); } };
        throw new Error("body");
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SuppressedError, true);
    shouldBe(caught.error.message, "dispose");
    shouldBe(caught.suppressed.message, "body");
}

async function testSyncFallbackThrows() {
    let caught;
    try {
        await using a = {
            [Symbol.dispose]() { throw new Error("sync dispose"); }
        };
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof Error, true);
    shouldBe(caught.message, "sync dispose");
}

async function testMethodLookupThrowNoAwait() {
    var order = [];
    async function inner() {
        order.push("start");
        try {
            await using x = { [Symbol.asyncDispose]: 42 };
            order.push("unreachable");
        } catch (e) {
            order.push("caught:" + e.constructor.name);
        }
        order.push("end");
    }
    var p = inner();
    order.push("after-call");
    await p;
    shouldBe(order.join(","), "start,caught:TypeError,end,after-call");
}


async function main() {
    await testDisposeThrows();
    await testDisposeRejects();
    await testSuppressedError();
    await testBodyThrowThenDisposeThrow();
    await testSyncFallbackThrows();
    await testMethodLookupThrowNoAwait();
}

main().then(() => {}, e => { print("FAIL: " + e); $vm.abort(); });
drainMicrotasks();
