function shouldBe(actual, expected, label) {
    if (actual !== expected)
        throw new Error('bad value for ' + label + ': ' + actual + ', expected ' + expected);
}

const results = {};

// Each case primes an own-realm async function through a promise that is
// settled by an internal microtask carrying the main realm's globalObject.
// The realm of the resolving function passed to a subsequently awaited
// thenable must remain the async function's realm.
function setup(label, body, makeSubject) {
    const other = createGlobalObject();
    other.MainFunction = Function;
    other.MainObject = Object;
    other.MainAggregateError = AggregateError;
    other.results = results;
    other.label = label;
    const { subject, settle } = makeSubject();
    other.subject = subject;
    new other.Function(`
        const realmOf = (value) => {
            const c = value.constructor;
            return c === Function || c === Object || c === AggregateError ? "own-realm"
                : c === MainFunction || c === MainObject || c === MainAggregateError ? "main-realm"
                : "unknown";
        };
        const observe = async (promise) => {
            await promise;
            await { then(f) { results[label] = realmOf(f); f(0); } };
            results[label + "-done"] = true;
        };
        ${body}
    `)();
    return settle;
}

function makePending() {
    let resolve;
    const promise = new Promise((r) => { resolve = r; });
    return { subject: promise, settle: () => { resolve(1); } };
}

function makePendingRejection() {
    let reject;
    const promise = new Promise((unused, r) => { reject = r; });
    return { subject: promise, settle: () => { reject(new Error("rejected")); } };
}

const settles = [];

// PromiseReactionJob: result promise of then() must not be settled with the
// foreign settle-time realm.
settles.push(setup("then-fn", `
    observe(Promise.prototype.then.call(subject, (x) => x));
`, makePending));

settles.push(setup("then-no-handler", `
    observe(Promise.prototype.then.call(subject));
`, makePending));

settles.push(setup("catch-fn", `
    observe(Promise.prototype.catch.call(subject, () => {}));
`, makePending));

// PromiseFinallyReactionJob / PromiseFinallyAwaitJob.
settles.push(setup("finally", `
    observe(Promise.prototype.finally.call(subject, () => {}));
`, makePending));

settles.push(setup("finally-promise-result", `
    observe(Promise.prototype.finally.call(subject, () => Promise.resolve(0)));
`, makePending));

// Combinator jobs, primed through a then()-poisoned own-realm promise.
settles.push(setup("all", `
    observe(Promise.all([Promise.prototype.then.call(subject)]));
`, makePending));

settles.push(setup("race", `
    observe(Promise.race([Promise.prototype.then.call(subject)]));
`, makePending));

settles.push(setup("all-settled", `
    observe(Promise.allSettled([Promise.prototype.then.call(subject)]));
`, makePending));

// PromiseAllSettledResolveJob creates the result objects: their realm is
// directly observable.
settles.push(setup("all-settled-result-object", `
    (async () => {
        const entries = await Promise.allSettled([Promise.prototype.then.call(subject)]);
        results[label] = realmOf(entries[0]);
        results[label + "-done"] = true;
    })();
`, makePending));

// PromiseAnyResolveJob creates the AggregateError: its realm is directly
// observable.
settles.push(setup("any-aggregate-error", `
    (async () => {
        try {
            await Promise.any([Promise.prototype.then.call(subject)]);
        } catch (error) {
            results[label] = realmOf(error);
        }
        results[label + "-done"] = true;
    })();
`, makePendingRejection));

// AsyncFromSyncIteratorContinue: for-await over a sync iterable of a
// poisoned own-realm promise.
settles.push(setup("async-from-sync", `
    (async () => {
        for await (const value of [Promise.prototype.then.call(subject)]) { }
        await { then(f) { results[label] = realmOf(f); f(0); } };
        results[label + "-done"] = true;
    })();
`, makePending));

// Async generator driver primed through a poisoned own-realm promise.
settles.push(setup("async-generator-driver", `
    async function* generate() {
        await Promise.prototype.then.call(subject);
        yield { then(f) { results[label] = realmOf(f); f(0); } };
    }
    (async () => {
        for await (const value of generate()) { }
        results[label + "-done"] = true;
    })();
`, makePending));

// Control: a fully own-realm chain stays own-realm.
settles.push(setup("own-realm-control", `
    let resolveLocal;
    const local = new Promise((r) => { resolveLocal = r; });
    observe(Promise.prototype.then.call(local, (x) => x));
    resolveLocal(1);
`, () => ({ subject: null, settle: () => {} })));

drainMicrotasks();
for (const settle of settles)
    settle();
drainMicrotasks();

for (const label of [
    "then-fn",
    "then-no-handler",
    "catch-fn",
    "finally",
    "finally-promise-result",
    "all",
    "race",
    "all-settled",
    "all-settled-result-object",
    "any-aggregate-error",
    "async-from-sync",
    "async-generator-driver",
    "own-realm-control",
]) {
    shouldBe(results[label], "own-realm", label);
    shouldBe(results[label + "-done"], true, label + "-done");
}
