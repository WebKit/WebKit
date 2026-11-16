// Regression test for https://bugs.webkit.org/show_bug.cgi?id=318366
//
// DFG/FTL LICM would hoist a loop-invariant tuple node (MapIteratorNext, produced by
// array-destructuring of a Set) out of a loop and re-run its abstract effects through the
// single-value accessors of AtTailAbstractState, hitting ASSERT(!node->isTuple()). Hoisting
// such a tuple node is legitimate (its inputs are loop-invariant and it does not write), so
// AtTailAbstractState::clearForNode must tolerate the tuple node's own slot, and LICM must
// skip only the ExtractFromTuple consumer (whose child is an edge to the tuple node).

function f(s, n) {
    let v2 = -1;
    while (n) {
        [, v2] = s;
        n--;
    }
    return v2;
}
noInline(f);

const s = new Set([10, 20, 30]);
for (let j = 0; j < testLoopCount; j++) {
    const result = f(s, 3);
    // Destructuring [, v2] = s takes the second element of the fresh iterator each time.
    if (result !== 20)
        throw new Error(`Bad result: ${result}`);
}

// The unbounded form from the bug report reaches the same hoisting path via loop OSR entry.
function g() {
    const v1 = new Set();
    let v2 = 0;
    let i = 0;
    while (8) {
        [, v2] = v1;
        if (++i >= testLoopCount)
            break;
    }
    return v2;
}
noInline(g);

if (g() !== undefined)
    throw new Error("Bad result from g");
