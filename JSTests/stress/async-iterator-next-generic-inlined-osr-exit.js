// Exercises op_async_iterator_next's generic (real next.call(iterator)) branch when the .next callee is
// DFG/FTL-inlined and repeatedly OSR-exits. Reconstructing that inlined call frame maps
// op_async_iterator_next to its LLInt return location (DFGOSRExitCompilerCommon.cpp, InlineCallFrame::Call
// case -> returnLocationThunk); op_async_iterator_next must therefore be listed in
// FOR_EACH_LLINT_OPCODE_WITH_RETURN or that thunk hits RELEASE_ASSERT_NOT_REACHED. The consumer's
// next() is a single shared monomorphic function (so it inlines); a value whose speculated type flips
// forces OSR exits inside the inlined next(). Runs clean across all tiers (esp. with the baseline JIT
// disabled, where DFG/FTL frames reconstruct straight to LLInt).

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

let addend = 1; // Int32 during warmup, later flipped to a double / object.

const iteratorProto = {
    next() {
        if (this.i >= 4)
            return { value: undefined, done: true };
        const value = addend + this.i; // yields addend+0 .. addend+3 (4 values)
        this.i++;
        return { value, done: false };
    }
};

function makeIterable() {
    return {
        [Symbol.asyncIterator]() {
            const it = Object.create(iteratorProto);
            it.i = 0;
            return it;
        }
    };
}

let done = false;
let error = null;

async function consume() {
    let sum = 0;
    for await (const x of makeIterable())
        sum += x;
    return sum;
}

async function main() {
    // Warm with Int32 addend: consume()/next() tier up, next() inlines and speculates Int32.
    let checksum = 0;
    for (let k = 0; k < testLoopCount; k++)
        checksum += await consume();
    assert(checksum === testLoopCount * (1 + 2 + 3 + 4), "int warmup checksum: " + checksum);

    // Flip the speculated type repeatedly to force OSR exits through the inlined next() frame.
    for (let round = 0; round < 40; round++) {
        addend = (round & 1) ? 1.5 : 1;
        for (let k = 0; k < 200; k++) {
            const s = await consume();
            assert(Number.isFinite(s), "finite sum, round " + round + ": " + s);
        }
    }

    // Object-with-valueOf addend: another speculation shape through the same frame.
    addend = { valueOf() { return 2; } };
    for (let k = 0; k < 300; k++) {
        const s = await consume();
        assert(s === (2 + 0) + (2 + 1) + (2 + 2) + (2 + 3), "object-addend sum: " + s);
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
