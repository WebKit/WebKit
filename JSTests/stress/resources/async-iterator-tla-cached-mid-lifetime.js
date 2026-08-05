// TLA module driver where the promise-then watchpoint is invalidated *mid-drive*: the first yields
// run on the fast path (the producer's cached iterator-result object is created and reused), then
// Object.prototype.then is defined partway through the for-await, so subsequent settles take the
// observable slow path. The driver must not leak the previously-cached object once it becomes
// observable -- every captured result must be distinct. Mirrors test 3 of
// JSTests/stress/async-generator-driver-cached-result-not-aliased.js for the module driver.
const captured = [];
const delivered = [];

async function* g() {
    yield "a";
    yield "b";
    yield "c";
    yield "d";
}

let n = 0;
for await (const x of g()) {
    delivered.push(x);
    if (++n === 2) {
        Object.defineProperty(Object.prototype, "then", {
            configurable: true,
            get() {
                if (this && typeof this === "object"
                    && Object.prototype.hasOwnProperty.call(this, "value")
                    && Object.prototype.hasOwnProperty.call(this, "done"))
                    captured.push(this);
                return undefined;
            },
        });
    }
}

delete Object.prototype.then;

globalThis.__tlaCachedMidLifetime = { delivered, captured };
export { };
