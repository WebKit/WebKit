// Array.prototype.sort must sort `undefined` elements to the tail per ECMA-262
// without passing them to the comparator. The inlined-sort fast path copies the
// receiver into a scratch butterfly during its compact phase; it must treat a
// Contiguous-array `undefined` the same as a hole (returning the sentinel and
// falling back to the slow path) so undefined ordering matches the spec.

function sortIt(a, c) { return a.sort(c); }

// A comparator that never triggers a BadType OSR exit on undefined (`<` / `>`
// coerce undefined to NaN, which is valid everywhere). Without the fix, the
// inlined fast path happily copies `undefined` into scratch and the insertion
// sort leaves it in place -- producing a wildly-wrong result.
function cmp(a, b) { return a < b ? -1 : a > b ? 1 : 0; }

function checkSortedWithUndefinedAtEnd(r, expectedNumbers, expectedUndefined, desc) {
    const totalLen = expectedNumbers.length + expectedUndefined;
    if (r.length !== totalLen)
        throw new Error(desc + ": wrong length " + r.length);
    for (let i = 0; i < expectedNumbers.length; ++i) {
        if (r[i] !== expectedNumbers[i])
            throw new Error(desc + ": wrong order at [" + i + "]: got " + r[i] + " expected " + expectedNumbers[i]);
    }
    for (let i = expectedNumbers.length; i < totalLen; ++i) {
        if (r[i] !== undefined)
            throw new Error(desc + ": expected undefined at [" + i + "] got " + r[i]);
        if (!(i in r))
            throw new Error(desc + ": undefined at [" + i + "] became a hole");
    }
}

// Length 7 (inside the 16-slot fast path), Contiguous, with interior undefined.
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt([5, undefined, 3, 1, undefined, 4, 2], cmp);
    checkSortedWithUndefinedAtEnd(r, [1, 2, 3, 4, 5], 2, "len7 iter " + i);
}

// A custom comparator that, if ever actually handed `undefined`, flags the error.
// The spec says the comparator is never called with undefined, so this must
// never throw regardless of tier.
function strictCmp(a, b) {
    if (a === undefined || b === undefined)
        throw new Error("comparator saw undefined");
    return a - b;
}
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt([3, undefined, 1, 2, undefined], strictCmp);
    checkSortedWithUndefinedAtEnd(r, [1, 2, 3], 2, "strict iter " + i);
}

// Length 2 fast path: pair of undefined must round-trip as a pair of undefined.
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt([undefined, undefined], cmp);
    checkSortedWithUndefinedAtEnd(r, [], 2, "pair iter " + i);
}

// Single undefined at the front of a Contiguous array.
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt([undefined, 3, 1, 2], cmp);
    checkSortedWithUndefinedAtEnd(r, [1, 2, 3], 1, "front iter " + i);
}

// Single undefined at the back.
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt([3, 1, 2, undefined], cmp);
    checkSortedWithUndefinedAtEnd(r, [1, 2, 3], 1, "back iter " + i);
}

// Dense Contiguous without any undefined: fast path must still produce a
// correct sort (regression guard on the new Contiguous-undefined branch).
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt(["b", "d", "a", "c"], cmp);
    if (r[0] !== "a" || r[1] !== "b" || r[2] !== "c" || r[3] !== "d")
        throw new Error("dense iter " + i + " wrong: " + JSON.stringify(r));
}
