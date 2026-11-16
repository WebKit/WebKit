function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected ' + expected);
}

const results = {};

function setup(label, makePromise) {
    const other = createGlobalObject();
    other.MainFunction = Function;
    other.results = results;
    other.label = label;
    const { promise, settle } = makePromise();
    other.subject = promise;
    new other.Function(`
        (async () => {
            await subject;
            await { then(f) {
                results[label] = f.constructor === Function ? "own-realm"
                    : f.constructor === MainFunction ? "main-realm"
                    : "unknown";
                f(0);
            } };
            results[label + "-done"] = true;
        })();
    `)();
    return settle;
}

// An async function awaiting a pending cross-realm vanilla promise must not have
// its continuation realm poisoned: the resolving functions of a subsequently
// awaited thenable must belong to the async function's realm.
const settlePending = setup("pending-vanilla", () => {
    let resolve;
    const promise = new Promise((r) => { resolve = r; });
    return { promise, settle: () => { resolve(1); } };
});

setup("fulfilled-vanilla", () => ({ promise: Promise.resolve(1), settle: () => {} }));

class SubPromise extends Promise {}
const settleSubclass = setup("pending-subclass", () => {
    let resolve;
    const promise = new SubPromise((r) => { resolve = r; });
    return { promise, settle: () => { resolve(1); } };
});

// Adopting a cross-realm promise through a resolving function must not leak the
// foreign realm into a same-realm promise that an async function is awaiting.
let settleMainForAdoption;
{
    const other = createGlobalObject();
    other.MainFunction = Function;
    other.results = results;
    const mainPending = new Promise((r) => { settleMainForAdoption = () => { r(2); }; });
    other.subject = mainPending;
    new other.Function(`
        let resolveLocal;
        const local = new Promise((r) => { resolveLocal = r; });
        (async () => {
            await local;
            await { then(f) {
                results["resolving-function-adoption"] = f.constructor === Function ? "own-realm"
                    : f.constructor === MainFunction ? "main-realm"
                    : "unknown";
                f(0);
            } };
            results["resolving-function-adoption-done"] = true;
        })();
        resolveLocal(subject);
    `)();
}

// Async generators drive awaited values through the same adoption path.
let settleMainForGenerator;
{
    const other = createGlobalObject();
    other.MainFunction = Function;
    other.results = results;
    const mainPending = new Promise((r) => { settleMainForGenerator = () => { r(3); }; });
    other.subject = mainPending;
    new other.Function(`
        async function* generate() {
            await subject;
            yield { then(f) {
                results["async-generator"] = f.constructor === Function ? "own-realm"
                    : f.constructor === MainFunction ? "main-realm"
                    : "unknown";
                f(0);
            } };
        }
        (async () => {
            for await (const value of generate()) { }
            results["async-generator-done"] = true;
        })();
    `)();
}

drainMicrotasks();
settlePending();
settleSubclass();
settleMainForAdoption();
settleMainForGenerator();
drainMicrotasks();

shouldBe(results["pending-vanilla"], "own-realm");
shouldBe(results["pending-vanilla-done"], true);
shouldBe(results["fulfilled-vanilla"], "own-realm");
shouldBe(results["fulfilled-vanilla-done"], true);
shouldBe(results["pending-subclass"], "own-realm");
shouldBe(results["pending-subclass-done"], true);
shouldBe(results["resolving-function-adoption"], "own-realm");
shouldBe(results["resolving-function-adoption-done"], true);
shouldBe(results["async-generator"], "own-realm");
shouldBe(results["async-generator-done"], true);
