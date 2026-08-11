//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

async function test() {
    var order = [];
    {
        await using a = {
            [Symbol.asyncDispose]() {
                order.push("dispose-a");
                return Promise.resolve();
            }
        };
        order.push("body");
    }
    order.push("after");
    shouldBe(order.join(","), "body,dispose-a,after");
}

async function testReverseOrder() {
    var order = [];
    {
        await using a = { [Symbol.asyncDispose]() { order.push("a"); } };
        await using b = { [Symbol.asyncDispose]() { order.push("b"); } };
        await using c = { [Symbol.asyncDispose]() { order.push("c"); } };
    }
    shouldBe(order.join(","), "c,b,a");
}

async function testNull() {
    var order = [];
    var wasSync = true;
    var promise = (async function() {
        {
            order.push("before");
            await using x = null;
            order.push("after-decl");
        }
        order.push("after-block");
        shouldBe(wasSync, false);
    })();
    wasSync = false;
    await promise;
    shouldBe(order.join(","), "before,after-decl,after-block");
}

async function testMixed() {
    var order = [];
    {
        await using a = { [Symbol.asyncDispose]() { order.push("async-a"); } };
        using b = { [Symbol.dispose]() { order.push("sync-b"); } };
        order.push("body");
    }
    shouldBe(order.join(","), "body,sync-b,async-a");
}

async function testSyncFallback() {
    var calledAsync = false;
    var calledSync = false;
    {
        await using a = {
            [Symbol.dispose]() { calledSync = true; }
        };
    }
    shouldBe(calledAsync, false);
    shouldBe(calledSync, true);
}

async function testAsyncDisposeReturnValueAwaited() {
    var resolved = false;
    {
        await using a = {
            [Symbol.asyncDispose]() {
                return new Promise(resolve => {
                    Promise.resolve().then(() => {
                        resolved = true;
                        resolve();
                    });
                });
            }
        };
    }
    shouldBe(resolved, true);
}

async function testNotEvaluatedNoAwait() {
    var wasSync = true;
    var promise = (async function() {
        outer: {
            if (true) break outer;
            await using x = null;
        }
        shouldBe(wasSync, true);
    })();
    wasSync = false;
    await promise;
}

async function main() {
    await test();
    await testReverseOrder();
    await testNull();
    await testMixed();
    await testSyncFallback();
    await testAsyncDisposeReturnValueAwaited();
    await testNotEvaluatedNoAwait();
}

main().then(() => {}, e => { print("FAIL: " + e); throw e; });
drainMicrotasks();
