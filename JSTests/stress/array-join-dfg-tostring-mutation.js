function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

// 1. ToString shrinks arr.length. Per spec (Array.prototype.join):
//    Step 2 captures len BEFORE step 4 ToString(separator). The loop iterates
//    over the captured len; mutated indices read as undefined and become "".
function joinShrink() {
    var arr = [1, 2, 3];
    var sep = { toString() { arr.length = 1; return ","; } };
    return arr.join(sep);
}
noInline(joinShrink);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinShrink(), "1,,");

// 2. ToString grows arr.length. Captured len is 3, so we ignore appended items.
function joinGrow() {
    var arr = [1, 2, 3];
    var sep = { toString() { arr.push(99, 100); return ","; } };
    return arr.join(sep);
}
noInline(joinGrow);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinGrow(), "1,2,3");

// 3. ToString causes Int32→Contiguous transition (storing a string forces the
//    indexing type to widen). Post-toString must revalidate the array mode.
function joinTransition() {
    var arr = [1, 2, 3];
    var sep = { toString() { arr[1] = "X"; return ","; } };
    return arr.join(sep);
}
noInline(joinTransition);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinTransition(), "1,X,3");

// 4. ToString punches a hole in the array and reads it via the empty-separator
//    fast path. The joiner must see the hole and fall back; result is "13".
function joinHolePunch() {
    var arr = [1, 2, 3];
    var sep = { toString() { delete arr[1]; return ""; } };
    return arr.join(sep);
}
noInline(joinHolePunch);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinHolePunch(), "13");

// 5. ToString replaces an Int32 element with a non-string/non-int32 value
//    (object). The empty-separator JSOnlyStringsAndInt32sJoiner must bail out
//    of its fast path and the slow path must still produce spec-correct output.
function joinNonStringElement() {
    var arr = [1, 2, 3];
    var obj = { toString() { return "OBJ"; } };
    var sep = { toString() { arr[1] = obj; return ""; } };
    return arr.join(sep);
}
noInline(joinNonStringElement);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinNonStringElement(), "1OBJ3");

// 6. ToString shrinks the array AND uses the empty-separator fast path. The
//    fast path reads butterfly[0, capturedLength); after shrink, slots beyond
//    the new length are conceptually undefined, so reading them via the joiner
//    is incorrect. The needsRevalidation path must detect length mismatch and
//    fall through to fastArrayJoin which handles it. Expected: "1".
function joinShrinkEmpty() {
    var arr = [1, 2, 3];
    var sep = { toString() { arr.length = 1; return ""; } };
    return arr.join(sep);
}
noInline(joinShrinkEmpty);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinShrinkEmpty(), "1");

// 7. ToString grows the array with empty separator. Captured length is 3, so
//    appended elements are ignored. Expected: "123".
function joinGrowEmpty() {
    var arr = [1, 2, 3];
    var sep = { toString() { arr.push(99, 100); return ""; } };
    return arr.join(sep);
}
noInline(joinGrowEmpty);
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinGrowEmpty(), "123");
