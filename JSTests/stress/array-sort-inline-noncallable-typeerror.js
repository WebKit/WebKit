// Per ECMAScript, Array.prototype.sort(comparator) must throw TypeError when the
// comparator is not undefined and not callable, even when the array is empty or
// length 1 so the comparator is never actually invoked. Before the fix, the DFG
// ArraySortIntrinsic speculated CellUse on the comparator and then, for an empty
// array, returned the receiver without a callability check. For length 1, the
// insertion sort header branched to Commit without a comparator call either.

function expectThrow(body, iter) {
    try { body(); }
    catch (e) {
        if (!(e instanceof TypeError))
            throw new Error("iter " + iter + ": expected TypeError, got " + e);
        return;
    }
    throw new Error("iter " + iter + ": expected TypeError, got nothing");
}

// Use a factory to get a non-COW empty / single-element array -- DFG's intrinsic
// skips COW arrays, which is why [] directly doesn't reproduce the bug.
function makeEmpty() { const a = [1]; a.pop(); return a; }
function makeSingle() { return [42]; }

function sortIt(a, c) { return a.sort(c); }

// Warm up with valid callable + empty so DFG inlines the intrinsic on ArrayWithInt32.
for (let i = 0; i < testLoopCount; ++i)
    sortIt(makeEmpty(), (a, b) => a - b);

// Non-callable cell on empty array -- used to silently succeed, must throw now.
for (let i = 0; i < testLoopCount; ++i)
    expectThrow(() => sortIt(makeEmpty(), {}), i);

// Length-1 array falls into the intrinsic's fast path but never calls the
// comparator from the insertion loop.
for (let i = 0; i < testLoopCount; ++i)
    expectThrow(() => sortIt(makeSingle(), {}), i);

// Symbols and other non-cell non-undefined values must also throw. These fall
// out of CellUse via OSR exit and the baseline handles them; re-asserting here
// catches any regression that would promote the intrinsic to accept them.
for (let i = 0; i < testLoopCount; ++i)
    expectThrow(() => sortIt(makeSingle(), Symbol.iterator), i);
for (let i = 0; i < testLoopCount; ++i)
    expectThrow(() => sortIt(makeEmpty(), null), i);
for (let i = 0; i < testLoopCount; ++i)
    expectThrow(() => sortIt(makeEmpty(), 42), i);

// Callable non-JSFunction cells (bound functions, native functions) remain valid
// and must not throw.
const bound = ((a, b) => a - b).bind(null);
for (let i = 0; i < testLoopCount; ++i) {
    if (sortIt([3, 1, 2], bound)[0] !== 1)
        throw new Error("iter " + i + ": bound comparator produced wrong order");
}

// undefined comparator uses the default compare and must not throw.
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt([3, 1, 2], undefined);
    if (r[0] !== 1 || r[2] !== 3)
        throw new Error("iter " + i + ": undefined comparator wrong order: " + r);
}
