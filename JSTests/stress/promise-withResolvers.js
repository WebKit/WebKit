function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function notReached() {
    throw new Error("should not reach here");
}

async function delay() {
    return new Promise((resolve, reject) => {
        setTimeout(resolve);
    });
}

shouldBe(Promise.withResolvers.name, "withResolvers");
shouldBe(Promise.withResolvers.length, 0);

{
    let {promise, resolve, reject} = Promise.withResolvers();

    shouldBe(promise instanceof Promise, true);

    shouldBe(resolve instanceof Function, true);
    shouldBe(resolve.name, "");
    shouldBe(resolve.length, 1);

    shouldBe(reject instanceof Function, true);
    shouldBe(reject.name, "");
    shouldBe(reject.length, 1);
}

async function test() {
    // withResolvers() returns functions in proper order.
    // 27.2.4.9 Promise.withResolvers Step 4-6
    // https://tc39.es/ecma262/2026/multipage/control-abstraction-objects.html#sec-promise.withResolvers
    {
        let ownKeys = Reflect.ownKeys(Promise.withResolvers());
        let expectedKeys = ["promise", "resolve", "reject"];
        shouldBe(ownKeys.length, expectedKeys.length);
        for (let i = 0; i < expectedKeys.length; i++) {
            shouldBe(ownKeys[i], expectedKeys[i]);
        }
    }

    // sync resolve
    {
        let {promise, resolve, reject} = Promise.withResolvers();
        resolve(42);
        reject("should not reject if already resolved");
        shouldBe(await promise, 42);
    }

    // async resolve
    {
        let {promise, resolve, reject} = Promise.withResolvers();
        await delay();
        resolve(42);
        await delay();
        reject("should not reject if already resolved");
        await delay();
        shouldBe(await promise, 42);
    }

    // sync reject
    {
        let {promise, resolve, reject} = Promise.withResolvers();
        reject(42);
        resolve("should not resolve if already rejected");
        try {
            await promise;
            notReached();
        } catch (e) {
            shouldBe(e, 42);
        }
    }

    // async reject
    {
        let {promise, resolve, reject} = Promise.withResolvers();
        await delay();
        reject(42);
        await delay();
        resolve("should not resolve if already rejected");
        await delay();
        try {
            await promise;
            notReached();
        } catch (e) {
            shouldBe(e, 42);
        }
    }
}

test().catch(function(error) {
    print(`FAIL: ${error}`);
    print(String(error.stack));
    $vm.abort();
});
drainMicrotasks();
