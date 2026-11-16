//@ requireOptions("--useExplicitResourceManagement=true")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

async function testAsyncGenerator() {
    var order = [];
    async function* gen() {
        await using a = { [Symbol.asyncDispose]() { order.push("dispose-a"); } };
        yield 1;
        yield 2;
        order.push("body-end");
    }
    var iter = gen();
    shouldBe((await iter.next()).value, 1);
    shouldBe((await iter.next()).value, 2);
    shouldBe((await iter.next()).done, true);
    shouldBe(order.join(","), "body-end,dispose-a");
}

async function testAsyncGeneratorReturn() {
    var order = [];
    async function* gen() {
        await using a = { [Symbol.asyncDispose]() { order.push("dispose-a"); } };
        yield 1;
        order.push("never");
    }
    var iter = gen();
    shouldBe((await iter.next()).value, 1);
    shouldBe((await iter.return(42)).value, 42);
    shouldBe(order.join(","), "dispose-a");
}

async function testAsyncGeneratorNestedBlocks() {
    var order = [];
    async function* gen() {
        {
            await using a = { [Symbol.asyncDispose]() { order.push("dispose-a"); } };
            {
                await using b = { [Symbol.asyncDispose]() { order.push("dispose-b"); } };
                yield 1;
            }
            order.push("between");
        }
        yield 2;
    }
    var iter = gen();
    shouldBe((await iter.next()).value, 1);
    shouldBe((await iter.next()).value, 2);
    shouldBe((await iter.next()).done, true);
    shouldBe(order.join(","), "dispose-b,between,dispose-a");
}

async function main() {
    await testAsyncGenerator();
    await testAsyncGeneratorReturn();
    await testAsyncGeneratorNestedBlocks();
}

main().then(() => {}, e => { print("FAIL: " + e); $vm.abort(); });
drainMicrotasks();
