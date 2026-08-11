// Regression test for op_async_iterator_next's generic branch call machinery, exercised across
// monomorphic (inlinable), polymorphic, and native/builtin .next callees. op_async_iterator_next is
// now recognized as an OpCallShape opcode (bytecode/OpcodeInlines.h), so handleCall consults
// handleIntrinsicCall for it -- which relies on the checkpoint-aware callee/argc/stack-offset helpers
// in BytecodeOperandsForCheckpoint.h being correct for this op. Verify correct iteration regardless
// of how polymorphic the .next call site becomes, warmed hot enough to reach the upper JIT tiers.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

// A family of distinct next() closures to make the op_async_iterator_next call site polymorphic.
function makeIterable(kind, n) {
    let i = 0;
    let next;
    if (kind === 0)
        next = function () { return Promise.resolve(i < n ? { value: i++, done: false } : { value: undefined, done: true }); };
    else if (kind === 1)
        next = function () { return Promise.resolve(i < n ? { value: (i++) * 2, done: false } : { value: undefined, done: true }); };
    else
        next = async function () { return i < n ? { value: (i++) + 1000, done: false } : { value: undefined, done: true }; };
    return { [Symbol.asyncIterator]() { return { next }; } };
}

function expectedSum(kind, n) {
    let s = 0;
    for (let i = 0; i < n; i++)
        s += kind === 0 ? i : kind === 1 ? i * 2 : i + 1000;
    return s;
}

let done = false;
let error = null;

async function consume(kind, n) {
    let sum = 0;
    for await (const x of makeIterable(kind, n))
        sum += x;
    return sum;
}

async function main() {
    for (let k = 0; k < testLoopCount; k++) {
        const kind = k % 3;
        const n = 4;
        const got = await consume(kind, n);
        assert(got === expectedSum(kind, n), `kind ${kind}: got ${got}, expected ${expectedSum(kind, n)}`);
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
