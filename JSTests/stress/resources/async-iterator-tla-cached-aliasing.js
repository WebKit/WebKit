// TLA module driving an async generator via the for-await fast-consumer path (module record as
// driver) while Object.prototype.then is defined. Defining `then` fires the promise-then
// watchpoint, so the settle path reads `.then` on each iterator result -- the result escapes to
// user code. The driver must therefore NOT hand out a reused/aliased cached object: each delivered
// result must be a distinct, unmutated { value, done } object (as if freshly created), exactly as
// required for the async-function/async-generator drivers (JSTests/stress/
// async-generator-driver-cached-result-not-aliased.js). This exercises that same safety for the
// module driver.
const captured = [];

Object.defineProperty(Object.prototype, "then", {
    configurable: true,
    get() {
        if (this && typeof this === "object"
            && Object.prototype.hasOwnProperty.call(this, "value")
            && Object.prototype.hasOwnProperty.call(this, "done"))
            captured.push({ result: this, value: this.value, done: this.done });
        return undefined;
    },
});

const delivered = [];
async function* g() {
    yield 10;
    yield 20;
    yield 30;
}

for await (const x of g())
    delivered.push(x);

delete Object.prototype.then;

globalThis.__tlaCachedAliasing = { delivered, captured };
export { };
