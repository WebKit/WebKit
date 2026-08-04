// Regression test for op_async_iterator_next's generic (real next.call(iterator)) branch when the
// .next callee is DFG/FTL-inlined and then OSR-exits. Reconstructing that inlined call frame on exit
// needs op_async_iterator_next mapped to its LLInt return location (DFGOSRExitCompilerCommon.cpp,
// InlineCallFrame::Call case); without it the exit jumps to a null target. We warm a for-await whose
// custom async iterator's next() is small and monomorphic (so it inlines), then flip a captured value's
// type so a speculation inside the inlined next() fails, forcing an OSR exit through that frame.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

let addend = 1; // starts Int32 so next()'s `x + addend` speculates Int32, then becomes a double.

function makeIterable(base) {
    let i = 0;
    return {
        [Symbol.asyncIterator]() {
            return {
                next() {
                    // Small, monomorphic, inlinable. The `base + addend` is speculated Int32 after
                    // warmup; once addend turns into a double the inlined body OSR-exits here.
                    let value = base + addend + i;
                    return Promise.resolve(i++ < 4 ? { value, done: false } : { value: undefined, done: true });
                }
            };
        }
    };
}

let done = false;
let error = null;
let checksum = 0;

async function consume(base) {
    let sum = 0;
    for await (const x of makeIterable(base))
        sum += x;
    return sum;
}

async function main() {
    // Warm up with Int32 addend so consume()/next() tier up and next() gets inlined.
    for (let k = 0; k < testLoopCount; k++)
        checksum += await consume(k & 0xff);

    // Now perturb the speculated type to force OSR exits inside the inlined next().
    addend = 1.5;
    for (let k = 0; k < testLoopCount; k++)
        checksum += await consume(k & 0xff);

    // A polymorphic-then-object perturbation for good measure.
    addend = { valueOf() { return 2; } };
    for (let k = 0; k < 200; k++)
        checksum += await consume(k & 0xff);

    assert(Number.isFinite(checksum), "checksum should be finite, was " + checksum);
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
