// Coverage for op_async_iterator_open's GENERIC path: for-await over async iterables that are NOT async
// generators, so the fast-path probe (which only fires for JSAsyncGeneratorType) never applies and the op
// runs its symbolCall (call @@asyncIterator) + getNext (get_by_id iterator.next) checkpoints, mirroring
// op_iterator_open. Warmed across tiers. Covers a custom iterator object, @@asyncIterator returning `this`,
// an observable (accessor) .next read exactly once, @@asyncIterator returning a non-object (TypeError from
// the getNext IsObject check), @@asyncIterator throwing, and async-from-sync (only @@iterator present).

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

async function assertThrowsAsync(fn, ctor, message) {
    try {
        await fn();
    } catch (e) {
        assert(e instanceof ctor, message + " (threw " + e + ")");
        return;
    }
    assert(false, message + " (did not throw)");
}

// Shared warmed for-await site.
async function consume(iterable) {
    let sum = 0;
    for await (const x of iterable)
        sum += x;
    return sum;
}

// @@asyncIterator returns a fresh custom iterator object.
function customIterable(n) {
    return {
        [Symbol.asyncIterator]() {
            let i = 0;
            return {
                next() {
                    return Promise.resolve(i < n ? { value: i++, done: false } : { value: undefined, done: true });
                }
            };
        }
    };
}

// @@asyncIterator returns `this` (the object is its own iterator).
function selfIterable(n) {
    return {
        i: 0,
        n,
        [Symbol.asyncIterator]() { return this; },
        next() {
            return Promise.resolve(this.i < this.n ? { value: this.i++, done: false } : { value: undefined, done: true });
        }
    };
}

let error = null;
let done = false;

async function main() {
    // Basic custom async iterator through the generic open path.
    for (let k = 0; k < testLoopCount; k++)
        assert(await consume(customIterable(4)) === 6, "customIterable"); // 0+1+2+3

    // @@asyncIterator returning this.
    for (let k = 0; k < testLoopCount; k++)
        assert(await consume(selfIterable(5)) === 10, "selfIterable"); // 0+1+2+3+4

    // Observable (accessor) .next on the iterator: getNext's get_by_id must read it exactly once.
    for (let k = 0; k < testLoopCount; k++) {
        let reads = 0;
        let realNext;
        const iterable = {
            [Symbol.asyncIterator]() {
                let i = 0;
                const iter = {
                    _next(...a) { return Promise.resolve(i < 3 ? { value: i++, done: false } : { value: undefined, done: true }); }
                };
                realNext = iter._next;
                Object.defineProperty(iter, "next", { configurable: true, get() { reads++; return realNext; } });
                return iter;
            }
        };
        assert(await consume(iterable) === 3, "accessorNext sum"); // 0+1+2
        assert(reads === 1, "accessorNext reads=" + reads);
    }

    // @@asyncIterator returns a non-object -> getNext IsObject check throws TypeError.
    for (let k = 0; k < testLoopCount; k++)
        await assertThrowsAsync(() => consume({ [Symbol.asyncIterator]() { return 42; } }), TypeError, "non-object iterator");

    // @@asyncIterator throws -> propagates out of the for-await.
    for (let k = 0; k < testLoopCount; k++)
        await assertThrowsAsync(() => consume({ [Symbol.asyncIterator]() { throw new RangeError("boom"); } }), RangeError, "throwing @@asyncIterator");

    // async-from-sync: only a SYNC @@iterator is present, so for-await wraps it in an async-from-sync iterator.
    for (let k = 0; k < testLoopCount; k++) {
        const syncIterable = {
            [Symbol.iterator]() {
                let i = 0;
                return { next() { return i < 4 ? { value: i++, done: false } : { value: undefined, done: true }; } };
            }
        };
        assert(await consume(syncIterable) === 6, "asyncFromSync"); // 0+1+2+3
    }

    // Polymorphic site: interleave the shapes so the open site sees many iterator structures.
    for (let k = 0; k < testLoopCount; k++) {
        assert(await consume(customIterable(3)) === 3, "poly custom");
        assert(await consume(selfIterable(3)) === 3, "poly self");
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
