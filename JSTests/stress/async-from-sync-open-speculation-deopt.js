// The DFG async_iterator_open fast paths speculate on @@asyncIterator's presence:
//   - AsyncFromSync-only sites Check(OtherUse): @@asyncIterator is absent (undefined/null).
//   - Generic sites that never profiled AsyncFromSync Check(CellUse): @@asyncIterator is present.
// This exercises both deopt transitions: after a site is compiled for one shape, feeding it the other must
// deopt to the baseline and still produce correct results (wrap a sync iterable, drive an async one) -- never a
// spurious TypeError or wrong sequence.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

function syncIterable(vals) {
    // A plain array: no @@asyncIterator, so for-await wraps it (AsyncFromSync).
    return vals.slice();
}

function asyncIterable(vals) {
    // A genuine async iterable (has @@asyncIterator, not a JSAsyncGenerator): drives the generic path.
    return {
        [Symbol.asyncIterator]() {
            let i = 0;
            return {
                next() {
                    return Promise.resolve(i < vals.length ? { value: vals[i++], done: false } : { value: undefined, done: true });
                }
            };
        }
    };
}

// Two distinct for-await sites so their iteration-mode profiles do not mix.
async function driveGenericFirst(iterable) {
    const out = [];
    for await (const x of iterable)
        out.push(x);
    return out.join(",");
}

async function driveAsyncFromSyncFirst(iterable) {
    const out = [];
    for await (const x of iterable)
        out.push(x);
    return out.join(",");
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;

    // Site 1: warm with async iterables (profiles Generic, never AsyncFromSync) so the DFG compiles the generic
    // path with Check(CellUse); then feed sync iterables -> @@asyncIterator absent -> CellUse deopts -> baseline
    // wraps the sync iterable. Must yield the array's values, not throw.
    for (let i = 0; i < N; i++)
        assert(await driveGenericFirst(asyncIterable([1, 2, 3])) === "1,2,3", "generic warm " + i);
    for (let i = 0; i < N; i++)
        assert(await driveGenericFirst(syncIterable([4, 5, 6])) === "4,5,6", "generic->sync switch " + i);

    // Site 2: warm with sync iterables (profiles AsyncFromSync only) so the DFG compiles Check(OtherUse); then
    // feed a genuine async iterable -> @@asyncIterator present -> OtherUse deopts -> baseline drives it.
    for (let i = 0; i < N; i++)
        assert(await driveAsyncFromSyncFirst(syncIterable([7, 8, 9])) === "7,8,9", "asyncFromSync warm " + i);
    for (let i = 0; i < N; i++)
        assert(await driveAsyncFromSyncFirst(asyncIterable([10, 11, 12])) === "10,11,12", "asyncFromSync->async switch " + i);
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
