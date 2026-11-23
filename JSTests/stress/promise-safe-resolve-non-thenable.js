// SafePromiseResolve with a value whose resolution cannot run user code settles the promise
// synchronously, exactly like an ordinary resolve.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

function settleSynchronously(value) {
    const { promise } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, value);
    let order = [];
    let fulfilled;
    promise.then((v) => { fulfilled = v; order.push("promise"); });
    Promise.resolve().then(() => order.push("next"));
    drainMicrotasks();
    shouldBe(order.join(), "promise,next");
    return fulfilled;
}

for (const value of [undefined, null, 0, 42, -0, NaN, "hello", true, Symbol("s"), 1n]) {
    const result = settleSynchronously(value);
    shouldBe(Object.is(result, value), true);
}

{
    const object = Object.create(null);
    shouldBe(settleSynchronously(object), object);
}

{
    const object = { };
    shouldBe(settleSynchronously(object), object);
}

{
    const object = { then: 42 };
    shouldBe(settleSynchronously(object), object);
}

{
    const object = Object.create({ then: "not callable" });
    shouldBe(settleSynchronously(object), object);
}

{
    const array = [1, 2, 3];
    shouldBe(settleSynchronously(array), array);
}

{
    const fn = function () { };
    shouldBe(settleSynchronously(fn), fn);
}

{
    const date = new Date;
    shouldBe(settleSynchronously(date), date);
}

{
    const typedArray = new Uint8Array(4);
    shouldBe(settleSynchronously(typedArray), typedArray);
}

{
    const stringObject = new String("then");
    shouldBe(settleSynchronously(stringObject), stringObject);
}

// An accessor without a getter reads as undefined without running user code.
{
    let setterCalled = false;
    const object = Object.defineProperty({ }, "then", { set(v) { setterCalled = true; } });
    shouldBe(settleSynchronously(object), object);
    const derived = Object.create(object);
    shouldBe(settleSynchronously(derived), derived);
    shouldBe(setterCalled, false);
}

// A non-callable "then" installed on Object.prototype is still inert.
{
    Object.prototype.then = 42;
    try {
        const object = { };
        shouldBe(settleSynchronously(object), object);
        const array = [];
        shouldBe(settleSynchronously(array), array);
    } finally {
        delete Object.prototype.then;
    }
}

// Dictionary-mode objects.
{
    const object = { };
    for (let i = 0; i < 1000; ++i)
        object["p" + i] = i;
    for (let i = 0; i < 1000; i += 2)
        delete object["p" + i];
    shouldBe(settleSynchronously(object), object);
}
