//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

async function testSyncThenAsync() {
    var order = [];
    {
        using a = { [Symbol.dispose]() { order.push("sync-a"); } };
        await using b = { [Symbol.asyncDispose]() { order.push("async-b"); } };
        order.push("body");
    }
    shouldBe(order.join(","), "body,async-b,sync-a");
}

async function testAsyncThenSync() {
    var order = [];
    {
        await using a = { [Symbol.asyncDispose]() { order.push("async-a"); } };
        using b = { [Symbol.dispose]() { order.push("sync-b"); } };
        order.push("body");
    }
    shouldBe(order.join(","), "body,sync-b,async-a");
}

async function testInterleaved() {
    var order = [];
    {
        using a = { [Symbol.dispose]() { order.push("sync-a"); } };
        await using b = { [Symbol.asyncDispose]() { order.push("async-b"); } };
        using c = { [Symbol.dispose]() { order.push("sync-c"); } };
        await using d = { [Symbol.asyncDispose]() { order.push("async-d"); } };
        order.push("body");
    }
    shouldBe(order.join(","), "body,async-d,sync-c,async-b,sync-a");
}

async function testMixedWithNullAwaitUsing() {
    var order = [];
    var wasSync = true;
    var p = (async function() {
        {
            using a = { [Symbol.dispose]() { order.push("sync-a"); } };
            await using b = null;
            order.push("body");
        }
        shouldBe(wasSync, false);
        order.push("after");
    })();
    wasSync = false;
    await p;
    shouldBe(order.join(","), "body,sync-a,after");
}

async function testMixedSuppressedError() {
    var caught;
    try {
        using a = { [Symbol.dispose]() { throw new Error("sync"); } };
        await using b = { [Symbol.asyncDispose]() { return Promise.reject(new Error("async")); } };
    } catch (e) {
        caught = e;
    }
    shouldBe(caught instanceof SuppressedError, true);
    shouldBe(caught.error.message, "sync");
    shouldBe(caught.suppressed.message, "async");
}

async function main() {
    await testSyncThenAsync();
    await testAsyncThenSync();
    await testInterleaved();
    await testMixedWithNullAwaitUsing();
    await testMixedSuppressedError();
}

main().then(() => {}, e => { print("FAIL: " + e); $vm.abort(); });
drainMicrotasks();
