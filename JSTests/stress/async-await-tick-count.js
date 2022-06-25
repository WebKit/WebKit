if (globalThis.console)
    globalThis.print = console.log.bind(console);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

async function returnDirectPrimitive() {
    return 1;
}

async function returnAwaitPrimitive() {
    return await 1;
}

async function returnDirectPromisePrimitive() {
    return Promise.resolve(1);
}

async function returnAwaitPromisePrimitive() {
    return await Promise.resolve(1);
}

const resolved = Promise.resolve();

async function test(fn, expected) {
    let done = false;
    let count = 0;
    fn().then(() => { done = true; });

    function counter() {
        if (done)
            shouldBe(count, expected);
        else {
            resolved.then(() => {
                count++;
                counter();
            });
        }
    }
    counter();
}

async function tests() {
    await resolved;
    await test(returnDirectPrimitive, 1);
    await test(returnAwaitPrimitive, 2);

    await test(returnDirectPromisePrimitive, 3);
    await test(returnAwaitPromisePrimitive, 2);
}

if (globalThis.setUnhandledRejectionCallback) {
    setUnhandledRejectionCallback(function (promise) {
        $vm.abort();
    });
}

tests();
