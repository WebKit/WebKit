// Tests that Array.prototype.concat's COW (copy-on-write) result path is correctly modeled
// in the DFG abstract interpreter. When one operand is empty and the other is a
// CopyOnWrite-butterflied array, concatAppendArray takes a fast path that reuses the source
// butterfly and returns a CopyOnWrite-indexed result. If the DFG's abstract result structure
// set omits CopyOnWrite variants, downstream speculation on the result can OSR-exit
// spuriously.

function assert(cond, msg) {
    if (!cond)
        throw new Error("Bad: " + (msg || ""));
}
noInline(assert);

function shallowEq(a, b) {
    assert(a.length === b.length, "length " + a.length + " vs " + b.length);
    for (let i = 0; i < a.length; i++)
        assert(a[i] === b[i], "index " + i);
}
noInline(shallowEq);

// Array literals like `[1, 2, 3]` produce CopyOnWriteArrayWithInt32 until written to. Keeping
// them unmodified throughout the test preserves the COW state and exercises the COW
// fast-path in concatAppendArray.
function firstEmpty() {
    return [].concat([1, 2, 3]);
}
noInline(firstEmpty);

function secondEmpty() {
    return [1, 2, 3].concat([]);
}
noInline(secondEmpty);

function firstEmptyDouble() {
    return [].concat([1.5, 2.5, 3.5]);
}
noInline(firstEmptyDouble);

function secondEmptyDouble() {
    return [1.5, 2.5, 3.5].concat([]);
}
noInline(secondEmptyDouble);

function firstEmptyContiguous() {
    return [].concat(["a", "b", "c"]);
}
noInline(firstEmptyContiguous);

function secondEmptyContiguous() {
    return ["a", "b", "c"].concat([]);
}
noInline(secondEmptyContiguous);

// Also exercise a downstream use of the result — iterating and summing — so that any
// speculation on the result's structure actually needs to be sound.
function sumAfterConcat(producer) {
    const arr = producer();
    let out = 0;
    for (let i = 0; i < arr.length; i++)
        out = out + arr[i];
    return out;
}
noInline(sumAfterConcat);

for (let i = 0; i < testLoopCount; i++) {
    shallowEq(firstEmpty(), [1, 2, 3]);
    shallowEq(secondEmpty(), [1, 2, 3]);
    shallowEq(firstEmptyDouble(), [1.5, 2.5, 3.5]);
    shallowEq(secondEmptyDouble(), [1.5, 2.5, 3.5]);
    shallowEq(firstEmptyContiguous(), ["a", "b", "c"]);
    shallowEq(secondEmptyContiguous(), ["a", "b", "c"]);

    assert(sumAfterConcat(firstEmpty) === 6, "sum firstEmpty");
    assert(sumAfterConcat(secondEmpty) === 6, "sum secondEmpty");
    assert(sumAfterConcat(firstEmptyDouble) === 7.5, "sum firstEmptyDouble");
    assert(sumAfterConcat(secondEmptyDouble) === 7.5, "sum secondEmptyDouble");
}

// Mutate the result to confirm it is a distinct-enough array (COW or not, the observable
// semantic is that writing to the result does not affect the source).
const source = [10, 20, 30];
const result = source.concat([]);
result[0] = 999;
assert(source[0] === 10, "source mutated");
assert(result[0] === 999, "result didn't mutate");
