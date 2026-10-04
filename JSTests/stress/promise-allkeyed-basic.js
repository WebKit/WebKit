//@ requireOptions("--usePromiseAllKeyed=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`bad error: ${String(error)}`);
    }
    if (!errorThrown)
        throw new Error("not thrown");
}

// Property attributes of Promise.allKeyed.
{
    let descriptor = Object.getOwnPropertyDescriptor(Promise, "allKeyed");
    shouldBe(typeof descriptor.value, "function");
    shouldBe(descriptor.writable, true);
    shouldBe(descriptor.enumerable, false);
    shouldBe(descriptor.configurable, true);
    shouldBe(Promise.allKeyed.length, 1);
    shouldBe(Promise.allKeyed.name, "allKeyed");
}

// Basic behavior: keys, values (promises and non-promises), null prototype result.
{
    let result;
    Promise.allKeyed({
        a: Promise.resolve(1),
        b: 2,
        c: new Promise((resolve) => resolve("three")),
    }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBeArray(Reflect.ownKeys(result), ["a", "b", "c"]);
    shouldBe(result.a, 1);
    shouldBe(result.b, 2);
    shouldBe(result.c, "three");
    let descriptor = Object.getOwnPropertyDescriptor(result, "a");
    shouldBe(descriptor.writable, true);
    shouldBe(descriptor.enumerable, true);
    shouldBe(descriptor.configurable, true);
}

// Empty object resolves to an empty null-prototype object.
{
    let result;
    Promise.allKeyed({}).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBe(Reflect.ownKeys(result).length, 0);
}

// Result key order: string keys in creation order, then symbols, regardless of resolution order.
{
    let symbol1 = Symbol("s1");
    let symbol2 = Symbol("s2");
    let resolvers = { };
    let input = { };
    input.beta = new Promise((resolve) => { resolvers.beta = resolve; });
    input[symbol1] = new Promise((resolve) => { resolvers[symbol1] = resolve; });
    input.alpha = new Promise((resolve) => { resolvers.alpha = resolve; });
    input[symbol2] = new Promise((resolve) => { resolvers[symbol2] = resolve; });

    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    resolvers[symbol2]("v-symbol2");
    resolvers.alpha("v-alpha");
    resolvers[symbol1]("v-symbol1");
    resolvers.beta("v-beta");
    drainMicrotasks();
    let keys = Reflect.ownKeys(result);
    shouldBe(keys.length, 4);
    shouldBe(keys[0], "beta");
    shouldBe(keys[1], "alpha");
    shouldBe(keys[2], symbol1);
    shouldBe(keys[3], symbol2);
    shouldBe(result.beta, "v-beta");
    shouldBe(result.alpha, "v-alpha");
    shouldBe(result[symbol1], "v-symbol1");
    shouldBe(result[symbol2], "v-symbol2");
}

// Non-enumerable and inherited properties are ignored. Enumerable symbols are included.
{
    let visible = Symbol("visible");
    let hidden = Symbol("hidden");
    let input = Object.create({ inherited: Promise.resolve("inherited") });
    input.own = Promise.resolve("own");
    input[visible] = Promise.resolve("visible");
    Object.defineProperty(input, "hiddenString", { value: Promise.resolve("no"), enumerable: false });
    Object.defineProperty(input, hidden, { value: Promise.resolve("no"), enumerable: false });

    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    drainMicrotasks();
    let keys = Reflect.ownKeys(result);
    shouldBe(keys.length, 2);
    shouldBe(keys[0], "own");
    shouldBe(keys[1], visible);
    shouldBe(result.own, "own");
    shouldBe(result[visible], "visible");
}

// Rejection: the first rejection rejects the combined promise; later settlements are ignored.
{
    let settled = [];
    let resolveA;
    let rejectB;
    let rejectC;
    Promise.allKeyed({
        a: new Promise((resolve) => { resolveA = resolve; }),
        b: new Promise((resolve, reject) => { rejectB = reject; }),
        c: new Promise((resolve, reject) => { rejectC = reject; }),
    }).then((value) => { settled.push(["fulfilled", value]); }, (reason) => { settled.push(["rejected", reason]); });
    rejectC("c-reason");
    rejectB("b-reason");
    resolveA("a-value");
    drainMicrotasks();
    shouldBe(settled.length, 1);
    shouldBe(settled[0][0], "rejected");
    shouldBe(settled[0][1], "c-reason");
}

// Non-object argument rejects (asynchronously) with a TypeError.
{
    for (let input of [undefined, null, 42, "string", Symbol("s"), true, 42n]) {
        let reason;
        let synchronousThrow = false;
        try {
            Promise.allKeyed(input).catch((error) => { reason = error; });
        } catch {
            synchronousThrow = true;
        }
        drainMicrotasks();
        shouldBe(synchronousThrow, false);
        shouldBe(reason instanceof TypeError, true);
    }
}

// |this| must be an object.
{
    shouldThrow(() => Promise.allKeyed.call(undefined, {}), "TypeError: |this| is not an object");
    shouldThrow(() => Promise.allKeyed.call(42, {}), "TypeError: |this| is not an object");
}

// Repeated calls produce independent result objects.
{
    let first;
    let second;
    Promise.allKeyed({ shared: 1 }).then((value) => { first = value; });
    Promise.allKeyed({ shared: 2 }).then((value) => { second = value; });
    drainMicrotasks();
    shouldBe(first.shared, 1);
    shouldBe(second.shared, 2);
    shouldBe(first !== second, true);
}

// Works as the operand of await.
{
    let result;
    (async () => {
        result = await Promise.allKeyed({ x: Promise.resolve("x"), y: 7 });
    })();
    drainMicrotasks();
    shouldBe(result.x, "x");
    shouldBe(result.y, 7);
}
