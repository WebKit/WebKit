//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldThrowAsync(run, errorType, message) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();
    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
}

async function* generator() {}
const AsyncIteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf(generator.prototype))

function test(thisObj) {
    return AsyncIteratorPrototype[Symbol.asyncDispose].call(thisObj);
}

{
    let result;
    let returnGetCount = 0;
    const obj = {
        get return() {
            returnGetCount++;
            return undefined;
        }
    };

    result = test(obj);
    shouldBe(result instanceof Promise, true);
    shouldBe(returnGetCount, 1);

    result = test(obj);
    shouldBe(result instanceof Promise, true);
    shouldBe(returnGetCount, 2);
}

{
    let result;
    let returnCallCount = 0;
    const obj = {
        return() {
            returnCallCount++;
        }
    };

    result = test(obj);
    shouldBe(result instanceof Promise, true);
    shouldBe(returnCallCount, 1);

    result = test(obj);
    shouldBe(result instanceof Promise, true);
    shouldBe(returnCallCount, 2);
}

{
    function OurError() {}
    const obj = {
        get return() {
            throw new OurError();
        }
    };
    shouldThrowAsync(async () => {
        const result = test(obj);
        shouldBe(result instanceof Promise, true);
        await result;
    }, OurError);
}

{
    function OurError() {}
    const obj = {
        return() {
            throw new OurError();
        }
    };
    shouldThrowAsync(async () => {
        const result = test(obj);
        shouldBe(result instanceof Promise, true);
        await result;
    }, OurError);
}

{
    function OurError() {}
    const obj = {
        async return() {
            throw new OurError();
        }
    };
    shouldThrowAsync(async () => {
        const result = test(obj);
        shouldBe(result instanceof Promise, true);
        await result;
    }, OurError);
}

{
    function OurError() {}
    const obj = {
        async return() {
            return 42;
        }
    };
    const result = test(obj);
    shouldBe(result instanceof Promise, true);
    let thenCalled = 0;
    result.then((value) => {
        thenCalled++;
        shouldBe(value, 42);
    });
    drainMicrotasks();
    shouldBe(thenCalled, 1);
}
