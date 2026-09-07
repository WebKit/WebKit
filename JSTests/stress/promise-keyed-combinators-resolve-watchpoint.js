//@ requireOptions("--usePromiseAllKeyed=1")

// Fires the Promise.resolve watchpoint. After that, even plain Promise.allKeyed /
// Promise.allSettledKeyed calls take the generic path: NewPromiseCapability + Get(this, "resolve")
// per call + one resolve call per enumerable key, all observable. Must stay a separate file:
// once the watchpoint fires it never rearms in this VM.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

const originalResolve = Promise.resolve;

// Replace Promise.resolve with an accessor to observe both the lookup and the calls.
let resolveGets = 0;
let resolveCalls = [];
Object.defineProperty(Promise, "resolve", {
    configurable: true,
    get() {
        resolveGets++;
        return function (value) {
            resolveCalls.push(value);
            return originalResolve.call(this, value);
        };
    },
});

// allKeyed goes through the generic path: resolve looked up once, called once per enumerable key.
{
    let input = { a: 1, b: originalResolve.call(Promise, 2) };
    Object.defineProperty(input, "hidden", { value: 3, enumerable: false });
    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(resolveGets, 1);
    shouldBe(resolveCalls.length, 2);
    shouldBe(resolveCalls[0], 1);
    shouldBe(result.a, 1);
    shouldBe(result.b, 2);
}

// allSettledKeyed likewise, with a rejection flowing into a rejected entry.
{
    resolveGets = 0;
    resolveCalls = [];
    let result;
    Promise.allSettledKeyed({ ok: 1, ng: originalResolve.call(Promise, 0).then(() => { throw "r"; }) }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(resolveGets, 1);
    shouldBe(resolveCalls.length, 2);
    shouldBe(result.ok.status, "fulfilled");
    shouldBe(result.ok.value, 1);
    shouldBe(result.ng.status, "rejected");
    shouldBe(result.ng.reason, "r");
}

// A plain data-property overwrite that transforms values is honored per key.
{
    delete Promise.resolve;
    Promise.resolve = function (value) {
        return originalResolve.call(this, typeof value === "number" ? value + 100 : value);
    };
    let result;
    Promise.allKeyed({ x: 1, y: 2 }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result.x, 101);
    shouldBe(result.y, 102);
}

// A throwing Promise.resolve rejects the combined promise with that exception.
{
    Promise.resolve = function () { throw new Error("resolve throws"); };
    let reason;
    Promise.allKeyed({ a: 1 }).catch((error) => { reason = error; });
    drainMicrotasks();
    shouldBe(String(reason), "Error: resolve throws");
    Promise.resolve = originalResolve;
}
