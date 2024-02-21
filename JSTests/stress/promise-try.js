//@ requireOptions("--usePromiseTryMethod=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function notReached() {
    throw new Error('should not reach here');
}

async function delay() {
    return new Promise((resolve, reject) => {
        setTimeout(resolve);
    });
}

shouldBe(Promise.try.length, 1);

async function test() {
    {
        let result = [];
        Promise.try(() => { result.push(1); });
        result.push(2);
        await delay();
        shouldBe(`${result}`, '1,2');
    }

    shouldBe(await Promise.try(() => Promise.resolve(3)), 3);

    try {
        await Promise.try(() => { throw 4; });
        notReached();
    } catch (e) {
        shouldBe(e, 4);
    }

    try {
        await Promise.try(() => Promise.reject(5));
        notReached();
    } catch (e) {
        shouldBe(e, 5);
    }

    shouldBe(await Promise.try((x, y, z) => x + y + z, 1, 2, 3), 6);
}

test().catch((error) => {
    print(`FAIL: ${error}\n${error.stack}`);
    $vm.abort();
});
drainMicrotasks();
