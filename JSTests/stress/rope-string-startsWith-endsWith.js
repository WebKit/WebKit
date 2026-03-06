function assert(condition, message) {
    if (!condition)
        throw new Error(message || "Assertion failed");
}

// Build a rope string that exceeds the 0x128 (296) length threshold.
function makeLongRope() {
    let a = "a".repeat(200);
    let b = "b".repeat(200);
    return a + b;
}

// Basic: rope startsWith with single character.
function testBasicRopeStartsWith() {
    let rope = makeLongRope();
    assert(rope.startsWith("a"), "startsWith 'a' should be true");
    assert(!rope.startsWith("b"), "startsWith 'b' should be false");
    assert(!rope.startsWith("z"), "startsWith 'z' should be false");
}

// Basic: rope endsWith with single character.
function testBasicRopeEndsWith() {
    let rope = makeLongRope();
    assert(rope.endsWith("b"), "endsWith 'b' should be true");
    assert(!rope.endsWith("a"), "endsWith 'a' should be false");
    assert(!rope.endsWith("z"), "endsWith 'z' should be false");
}

// startsWith with position argument.
function testRopeStartsWithPosition() {
    let rope = makeLongRope();
    assert(rope.startsWith("a", 0), "startsWith 'a' at 0");
    assert(rope.startsWith("a", 100), "startsWith 'a' at 100");
    assert(rope.startsWith("a", 199), "startsWith 'a' at 199");
    assert(!rope.startsWith("a", 200), "startsWith 'a' at 200 should be false");
    assert(rope.startsWith("b", 200), "startsWith 'b' at 200");
    assert(rope.startsWith("b", 399), "startsWith 'b' at 399");
    assert(!rope.startsWith("a", 400), "startsWith 'a' at length should be false");
}

// endsWith with endPosition argument.
function testRopeEndsWithEndPosition() {
    let rope = makeLongRope();
    assert(rope.endsWith("b", 400), "endsWith 'b' at 400");
    assert(rope.endsWith("b", 201), "endsWith 'b' at 201");
    assert(rope.endsWith("a", 200), "endsWith 'a' at 200");
    assert(rope.endsWith("a", 1), "endsWith 'a' at 1");
    assert(!rope.endsWith("b", 200), "endsWith 'b' at 200 should be false");
    assert(!rope.endsWith("a", 0), "endsWith 'a' at 0 should be false");
}

// Three-fiber rope.
function testThreeFiberRope() {
    let a = "a".repeat(100);
    let b = "b".repeat(100);
    let c = "c".repeat(100);
    let rope = a + b + c; // 300 chars, JSC may create a 3-fiber rope
    assert(rope.startsWith("a"), "3-fiber: startsWith 'a'");
    assert(!rope.startsWith("b"), "3-fiber: startsWith 'b' should be false");
    assert(rope.startsWith("b", 100), "3-fiber: startsWith 'b' at 100");
    assert(rope.startsWith("c", 200), "3-fiber: startsWith 'c' at 200");
    assert(rope.endsWith("c"), "3-fiber: endsWith 'c'");
    assert(!rope.endsWith("b"), "3-fiber: endsWith 'b' should be false");
    assert(rope.endsWith("b", 200), "3-fiber: endsWith 'b' at 200");
    assert(rope.endsWith("a", 100), "3-fiber: endsWith 'a' at 100");
}

// Substring rope as root: slice() creates a substring rope.
function testSubstringRopeRoot() {
    let base = "a".repeat(200) + "b".repeat(200);
    // Force resolve so slice() creates a substring rope over a resolved base.
    base.charCodeAt(0);
    let sub = base.slice(100, 350); // 250 chars, substring rope
    assert(sub.startsWith("a"), "sub: startsWith 'a'");
    assert(!sub.startsWith("b"), "sub: startsWith 'b' should be false");
    assert(sub.startsWith("b", 100), "sub: startsWith 'b' at 100");
    assert(sub.endsWith("b"), "sub: endsWith 'b'");
    assert(!sub.endsWith("a"), "sub: endsWith 'a' should be false");
    assert(sub.endsWith("a", 100), "sub: endsWith 'a' at 100");
}

// Substring rope as fiber: concat a substring with another string.
function testSubstringRopeFiber() {
    let base = "x".repeat(400);
    base.charCodeAt(0);
    let sub = base.slice(0, 200); // substring rope, 200 chars of 'x'
    let rope = sub + "y".repeat(200); // fiber0=substring rope, fiber1=resolved
    assert(rope.startsWith("x"), "fiber sub: startsWith 'x'");
    assert(!rope.startsWith("y"), "fiber sub: startsWith 'y' should be false");
    assert(rope.endsWith("y"), "fiber sub: endsWith 'y'");
    assert(!rope.endsWith("x"), "fiber sub: endsWith 'x' should be false");
    assert(rope.startsWith("y", 200), "fiber sub: startsWith 'y' at 200");
    assert(rope.endsWith("x", 200), "fiber sub: endsWith 'x' at 200");
}

// Multi-character search strings should still work (not affected by optimization).
function testMultiCharSearch() {
    let rope = makeLongRope();
    assert(rope.startsWith("aaa"), "multi: startsWith 'aaa'");
    assert(!rope.startsWith("bbb"), "multi: startsWith 'bbb' should be false");
    assert(rope.endsWith("bbb"), "multi: endsWith 'bbb'");
    assert(!rope.endsWith("aaa"), "multi: endsWith 'aaa' should be false");
}

// Empty search string edge case.
function testEmptySearch() {
    let rope = makeLongRope();
    assert(rope.startsWith(""), "startsWith empty string should be true");
    assert(rope.endsWith(""), "endsWith empty string should be true");
    assert(rope.startsWith("", 200), "startsWith empty at 200 should be true");
    assert(rope.endsWith("", 0), "endsWith empty at 0 should be true");
}

// 16-bit character test.
function testSixteenBit() {
    let a = "\u{1234}".repeat(200);
    let b = "\u{5678}".repeat(200);
    let rope = a + b;
    assert(rope.startsWith("\u{1234}"), "16-bit: startsWith U+1234");
    assert(!rope.startsWith("\u{5678}"), "16-bit: startsWith U+5678 should be false");
    assert(rope.endsWith("\u{5678}"), "16-bit: endsWith U+5678");
    assert(!rope.endsWith("\u{1234}"), "16-bit: endsWith U+1234 should be false");
}

// 8-bit and 16-bit mixed rope.
function testMixed8And16Bit() {
    let ascii = "a".repeat(200); // 8-bit
    let wide = "\u{00FF}".repeat(200); // 8-bit Latin1
    let rope = ascii + wide;
    assert(rope.startsWith("a"), "mixed: startsWith 'a'");
    assert(rope.endsWith("\u{00FF}"), "mixed: endsWith U+00FF");
    assert(!rope.startsWith("\u{00FF}"), "mixed: startsWith U+00FF should be false");
    assert(!rope.endsWith("a"), "mixed: endsWith 'a' should be false");
}

// Short rope (below threshold) should still work correctly.
function testShortRope() {
    let rope = "hello" + " world";
    assert(rope.startsWith("h"), "short: startsWith 'h'");
    assert(!rope.startsWith("x"), "short: startsWith 'x' should be false");
    assert(rope.endsWith("d"), "short: endsWith 'd'");
    assert(!rope.endsWith("x"), "short: endsWith 'x' should be false");
}

// Nested rope: fiber0 is an unresolved non-substring rope.
// tryGetCharAt should bail out and fall through to the existing path.
function testNestedRope() {
    let partA = "a".repeat(500);
    let partB = "b".repeat(500);
    let inner = partA + partB; // 1000 chars, rope
    let partC = "c".repeat(200);
    let rope = inner + partC; // 1200 chars, fiber0 = inner (rope)
    // startsWith should still work (may or may not hit fast path)
    assert(rope.startsWith("a"), "nested: startsWith 'a'");
    assert(!rope.startsWith("c"), "nested: startsWith 'c' should be false");
    // endsWith should use backward direction and find partC
    assert(rope.endsWith("c"), "nested: endsWith 'c'");
    assert(!rope.endsWith("a"), "nested: endsWith 'a' should be false");
}

for (let i = 0; i < testLoopCount; i++) {
    testBasicRopeStartsWith();
    testBasicRopeEndsWith();
    testRopeStartsWithPosition();
    testRopeEndsWithEndPosition();
    testThreeFiberRope();
    testSubstringRopeRoot();
    testSubstringRopeFiber();
    testMultiCharSearch();
    testEmptySearch();
    testSixteenBit();
    testMixed8And16Bit();
    testShortRope();
    testNestedRope();
}
