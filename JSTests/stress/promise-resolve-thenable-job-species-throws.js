// ECMA-262 27.2.2.2 NewPromiseResolveThenableJob step c: if calling `then`
// throws (here, SpeciesConstructor on the resolution promise), the promise
// being resolved must be rejected with that error.
//
// The constructor is poisoned per-instance so Promise.prototype.then and the
// global species watchpoint stay intact and the fast path is taken.

function shouldBe(a, b) { if (a !== b) throw new Error(`FAIL: ${a} !== ${b}`); }

const speciesError = new Error("species-boom");
const inner = Promise.resolve();
inner.constructor = {
    get [Symbol.species]() { throw speciesError; }
};

let result = "pending";
new Promise(resolve => resolve(inner)).then(
    () => { result = "fulfilled"; },
    e => { result = e; }
);

drainMicrotasks();
shouldBe(result, speciesError);

// Twin: the await/async-generator path (PromiseResolveThenableJobWithInternalMicrotaskFastSlow).
let awaitResult = "pending";
(async () => {
    try {
        const inner2 = Promise.resolve();
        inner2.constructor = {
            get [Symbol.species]() { throw speciesError; }
        };
        await inner2;
        awaitResult = "fulfilled";
    } catch (e) {
        awaitResult = e;
    }
})();

drainMicrotasks();
shouldBe(awaitResult, speciesError);
