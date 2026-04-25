//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function shouldResolve(p, expected) {
    let status, value;
    p.then(v => { status = "resolved"; value = v; }, e => { status = "rejected"; value = e; });
    drainMicrotasks();
    if (status !== "resolved")
        throw new Error(`Expected resolved but got ${status}: ${value}`);
    shouldBe(value, expected);
}

// %AsyncIteratorPrototype%[@@asyncDispose] uses GetMethod(O, "return"),
// which treats null the same as undefined. When `return` is null, the
// promise should resolve with undefined rather than rejecting.

async function* ag() {}
const AsyncIteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf(Object.getPrototypeOf(ag())));

// return: null → resolved undefined
{
    const iter = Object.create(AsyncIteratorPrototype);
    iter.return = null;
    shouldResolve(iter[Symbol.asyncDispose](), undefined);
}

// return: undefined → resolved undefined (existing)
{
    const iter = Object.create(AsyncIteratorPrototype);
    iter.return = undefined;
    shouldResolve(iter[Symbol.asyncDispose](), undefined);
}

// return: callable → still invoked
{
    let called = false;
    const iter = Object.create(AsyncIteratorPrototype);
    iter.return = function() { called = true; return {}; };
    shouldResolve(iter[Symbol.asyncDispose](), undefined);
    shouldBe(called, true);
}

// await using with async iterator whose return is null
{
    let caught;
    (async () => {
        async function* gen() { yield 1; yield 2; }
        const iter = gen();
        iter.return = null;
        {
            await using x = iter;
        }
    })().catch(e => caught = e);
    drainMicrotasks();
    shouldBe(caught, undefined);
}
