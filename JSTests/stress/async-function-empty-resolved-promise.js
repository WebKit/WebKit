//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual} expected ${expected}`);
}

// Empty async function: must hit the `return @newResolvedPromise(undefined)` short-circuit.
async function empty() {}
async function emptyArrow() {}
class C { async emptyMethod() {} }
const obj = { async emptyShort() {} };

// `await using` declaration is still classified as "without await" (no AwaitExpression),
// but hasStatements() is true, so we must NOT take the empty short-circuit — the disposal
// stack still has to run.
let disposedSync = 0;
let disposedAsync = 0;
async function onlyUsing() {
    using x = { [Symbol.dispose]() { disposedSync++; } };
}
async function onlyAwaitUsing() {
    await using y = { [Symbol.asyncDispose]() { disposedAsync++; return Promise.resolve(); } };
}

// No-await body that returns a thenable: the resolved promise must pipe from the thenable,
// not return it as-is (spec compliance — async function wrapper allocates its own promise).
async function returnsThenable() {
    return { then(r) { r(42); } };
}

// No-await body that throws: must return a rejected promise with the thrown value.
async function throwsLiteral() { throw "boom"; }
async function throwsError() { throw new Error("e"); }

// No-await body with default arg that throws — crashes without the param-catch fix.
function thrower() { throw new Error("default-arg"); }
async function defaultThrower(arg = thrower()) {}
async function defaultThrowerBody(arg = thrower()) { return arg; }

async function run() {
    shouldBe(await empty(), undefined);
    shouldBe(await emptyArrow(), undefined);
    shouldBe(await new C().emptyMethod(), undefined);
    shouldBe(await obj.emptyShort(), undefined);

    shouldBe(await onlyUsing(), undefined);
    shouldBe(disposedSync, 1);

    shouldBe(await onlyAwaitUsing(), undefined);
    shouldBe(disposedAsync, 1);

    // Thenable pipes through — final resolution is 42, not the thenable object.
    shouldBe(await returnsThenable(), 42);

    // The wrapper promise must be a brand-new promise, not === to any returned inner promise.
    const inner = Promise.resolve(7);
    async function returnsPromise() { return inner; }
    const outer = returnsPromise();
    if (outer === inner)
        throw new Error("wrapper must not return the input promise");
    shouldBe(await outer, 7);

    // Rejections from throws and default-arg throws.
    let caught;
    try { await throwsLiteral(); } catch (e) { caught = e; }
    shouldBe(caught, "boom");

    try { await throwsError(); caught = null; } catch (e) { caught = e.message; }
    shouldBe(caught, "e");

    try { await defaultThrower(); caught = null; } catch (e) { caught = e.message; }
    shouldBe(caught, "default-arg");

    try { await defaultThrowerBody(); caught = null; } catch (e) { caught = e.message; }
    shouldBe(caught, "default-arg");
}

// Drive enough iterations to tier up through DFG and FTL.
(async () => {
    for (let i = 0; i < 1000; i++)
        await run();
})().then(() => {}, e => { throw e; });
