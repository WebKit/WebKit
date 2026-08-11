function assert(b, ...m) {
    if (!b)
        throw new Error("Bad: ", ...m);
}
noInline(assert);

function shallowEq(a, b) {
    assert(a.length === b.length, a, b);
    for (let i = 0; i < a.length; i++)
        assert(a[i] === b[i], a, b);
}
noInline(shallowEq);

function runConcat(a, b) {
    return a.concat(b);
}
noInline(runConcat);

// Warm up and verify across a variety of indexing types.
function testBasic() {
    // Int32 + Int32
    shallowEq(runConcat([1, 2, 3], [4, 5, 6]), [1, 2, 3, 4, 5, 6]);
    // Int32 + Double
    shallowEq(runConcat([1, 2, 3], [4.5, 5.5, 6.5]), [1, 2, 3, 4.5, 5.5, 6.5]);
    // Double + Int32
    shallowEq(runConcat([1.5, 2.5, 3.5], [4, 5, 6]), [1.5, 2.5, 3.5, 4, 5, 6]);
    // Double + Double
    shallowEq(runConcat([1.5, 2.5], [3.5, 4.5]), [1.5, 2.5, 3.5, 4.5]);
    // Contiguous + Contiguous
    shallowEq(runConcat(["a", "b"], ["c", "d"]), ["a", "b", "c", "d"]);
    // Contiguous + Int32 (promoted to Contiguous)
    shallowEq(runConcat(["a"], [1, 2]), ["a", 1, 2]);
    // Empty + Int32
    shallowEq(runConcat([], [1, 2, 3]), [1, 2, 3]);
    // Int32 + Empty
    shallowEq(runConcat([1, 2, 3], []), [1, 2, 3]);
    // Empty + Empty
    shallowEq(runConcat([], []), []);
}

for (let i = 0; i < testLoopCount; i++) {
    testBasic();
}

// Invalidation tests: setting Symbol.isConcatSpreadable or subclassing must not break correctness.
function testSpeciesInvalidation() {
    class MyArray extends Array {
        static get [Symbol.species]() {
            return Array;
        }
    }
    const arr = new MyArray(1, 2, 3);
    // This goes through the host function because the receiver isn't an original Array structure.
    const r = arr.concat([4, 5]);
    shallowEq([...r], [1, 2, 3, 4, 5]);
}
testSpeciesInvalidation();

// Mutate Symbol.isConcatSpreadable on an instance — the intrinsic watchpoint should fire.
function testIsConcatSpreadable() {
    const a = [1, 2, 3];
    const b = [4, 5];
    // Baseline result before any spreading override.
    shallowEq(a.concat(b), [1, 2, 3, 4, 5]);
    b[Symbol.isConcatSpreadable] = false;
    // After the override, b should be concatenated as an element, not spread.
    const r = a.concat(b);
    assert(r.length === 4);
    assert(r[0] === 1);
    assert(r[3] === b);
}
testIsConcatSpreadable();

// COW (copy-on-write) arrays as operands.
function testCOW() {
    function makeCOW() { return [1, 2, 3]; }
    noInline(makeCOW);
    const a = makeCOW();
    const b = makeCOW();
    shallowEq(runConcat(a, b), [1, 2, 3, 1, 2, 3]);
}
for (let i = 0; i < testLoopCount; i++) {
    testCOW();
}

// Larger arrays exercising the memcpy fast path.
function testLarge() {
    const a = new Array(100);
    const b = new Array(100);
    for (let i = 0; i < 100; i++) { a[i] = i; b[i] = 100 + i; }
    const r = runConcat(a, b);
    assert(r.length === 200);
    for (let i = 0; i < 100; i++) {
        assert(r[i] === i);
        assert(r[100 + i] === 100 + i);
    }
}
for (let i = 0; i < testLoopCount; i++) {
    testLarge();
}

// array.concat(primitive) — should hit the ArrayConcatAppendOne fast path.
function appendInt(a) { return a.concat(42); }
function appendDouble(a) { return a.concat(3.14); }
function appendString(a) { return a.concat("x"); }
function appendBool(a) { return a.concat(true); }
function appendNull(a) { return a.concat(null); }
function appendUndef(a) { return a.concat(undefined); }
noInline(appendInt); noInline(appendDouble); noInline(appendString);
noInline(appendBool); noInline(appendNull); noInline(appendUndef);

for (let i = 0; i < testLoopCount; i++) {
    shallowEq(appendInt([1, 2, 3]), [1, 2, 3, 42]);
    shallowEq(appendDouble([1.5, 2.5]), [1.5, 2.5, 3.14]);
    shallowEq(appendString(["a", "b"]), ["a", "b", "x"]);
    shallowEq(appendBool([1, 2]), [1, 2, true]);
    shallowEq(appendNull([1]), [1, null]);

    const r = appendUndef([1, 2]);
    assert(r.length === 3);
    assert(r[2] === undefined);
}

// Polymorphic argument (sometimes array, sometimes primitive) — intrinsic should bail and the
// host function handles it correctly.
function polyConcat(a, b) { return a.concat(b); }
noInline(polyConcat);
for (let i = 0; i < testLoopCount; i++) {
    const even = (i & 1) === 0;
    const r = polyConcat([1, 2, 3], even ? [4, 5] : 42);
    if (even) shallowEq(r, [1, 2, 3, 4, 5]);
    else shallowEq(r, [1, 2, 3, 42]);
}

// Non-array object argument — intrinsic should bail, host function handles.
function objConcat(a, b) { return a.concat(b); }
noInline(objConcat);
const plainObj = { foo: "bar" };
for (let i = 0; i < testLoopCount; i++) {
    const r = objConcat([1, 2], plainObj);
    assert(r.length === 3);
    assert(r[2] === plainObj);
}
