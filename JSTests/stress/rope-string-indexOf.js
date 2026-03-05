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

// Basic: rope indexOf with single character.
function testBasicRopeIndexOf() {
    let rope = makeLongRope();
    assert(rope.indexOf("a") === 0, "first 'a' should be at 0");
    assert(rope.indexOf("b") === 200, "first 'b' should be at 200");
    assert(rope.indexOf("z") === -1, "'z' should not be found");
}

// startPosition argument.
function testRopeIndexOfWithPosition() {
    let rope = makeLongRope();
    assert(rope.indexOf("a", 100) === 100, "'a' from 100 should be at 100");
    assert(rope.indexOf("a", 200) === -1, "'a' from 200 should not be found");
    assert(rope.indexOf("b", 300) === 300, "'b' from 300 should be at 300");
    assert(rope.indexOf("b", 400) === -1, "'b' from 400 should not be found");
}

// Deep rope (multi-level concatenation).
function testDeepRope() {
    let s = "x".repeat(100);
    for (let i = 0; i < 10; i++)
        s = s + "y".repeat(30);
    // s is "x"*100 + "y"*300 = 400 chars, exceeds threshold
    assert(s.indexOf("x") === 0, "first 'x' at 0");
    assert(s.indexOf("y") === 100, "first 'y' at 100");
}

// Edge: character at fiber boundary.
function testFiberBoundary() {
    let a = "a".repeat(150);
    let b = "b" + "a".repeat(149);
    let rope = a + b; // 300 chars, 'b' is at position 150
    assert(rope.indexOf("b") === 150, "'b' at fiber boundary should be at 150");
}

// Three-fiber rope.
function testThreeFiberRope() {
    let a = "a".repeat(100);
    let b = "b".repeat(100);
    let c = "c".repeat(100);
    let rope = a + b + c; // JSC may create a 3-fiber rope
    assert(rope.indexOf("a") === 0);
    assert(rope.indexOf("b") === 100);
    assert(rope.indexOf("c") === 200);
    assert(rope.indexOf("c", 250) === 250);
    assert(rope.indexOf("z") === -1);
}

// Last character.
function testLastChar() {
    let a = "a".repeat(299);
    let rope = a + "z"; // 300 chars
    assert(rope.indexOf("z") === 299);
}

// Ensure short ropes still work (they take the normal path).
function testShortRope() {
    let rope = "hello" + " world";
    assert(rope.indexOf("w") === 6);
    assert(rope.indexOf("z") === -1);
}

// Substring rope as root: slice() creates a substring rope.
function testSubstringRopeRoot() {
    let base = "a".repeat(200) + "b".repeat(200);
    // Force resolve so slice() creates a substring rope over a resolved base.
    base.charCodeAt(0);
    let sub = base.slice(100, 350); // 250 chars, substring rope
    assert(sub.indexOf("a") === 0, "sub: first 'a' at 0");
    assert(sub.indexOf("b") === 100, "sub: first 'b' at 100");
    assert(sub.indexOf("z") === -1, "sub: 'z' not found");
    assert(sub.indexOf("b", 150) === 150, "sub: 'b' from 150 at 150");
}

// Substring rope as fiber: concat a substring with another string.
function testSubstringRopeFiber() {
    let base = "x".repeat(400);
    base.charCodeAt(0);
    let sub = base.slice(0, 200); // substring rope, 200 chars of 'x'
    let rope = sub + "y".repeat(200); // fiber0=substring rope, fiber1=resolved
    assert(rope.indexOf("x") === 0, "fiber sub: first 'x' at 0");
    assert(rope.indexOf("y") === 200, "fiber sub: first 'y' at 200");
    assert(rope.indexOf("z") === -1, "fiber sub: 'z' not found");
    assert(rope.indexOf("x", 199) === 199, "fiber sub: last 'x' at 199");
    assert(rope.indexOf("x", 200) === -1, "fiber sub: no 'x' from 200");
}

// Nested rope with startPosition: ensure startPosition is not regressed
// when tryFindOneChar bails out on a non-substring rope fiber.
// Regression test for: https://github.com/WebKit/WebKit/pull/58116#discussion_r2786165212
function makeNestedRope() {
    // Build rope = (partA + partB) + partC
    //   fiber0 = inner rope (partA + partB), 1000 chars, not resolved
    //   fiber1 = partC, 200 chars, resolved
    // 'z' is at position 50 inside partA.
    // tryFindOneChar sees fiber0 is a non-substring rope and bails out.
    // Bug: startPosition is set to offset (0), which is smaller than the
    //      original startPosition (e.g. 100). The fallback then searches
    //      from 0 and incorrectly finds 'z' at 50.
    let partA = "a".repeat(50) + "z" + "a".repeat(449); // 500 chars, 'z' at index 50
    let partB = "a".repeat(500);                          // 500 chars
    let inner = partA + partB;                             // 1000 chars, rope
    let partC = "b".repeat(200);                           // 200 chars
    return inner + partC;                                  // 1200 chars, fiber0 = inner (rope)
}
function testNestedRopeStartPosition() {
    // Each assertion uses a fresh rope because the first indexOf call
    // resolves the rope, hiding the bug for subsequent calls.
    assert(makeNestedRope().indexOf("z", 100) === -1, "nested rope: indexOf('z', 100) should be -1");
    assert(makeNestedRope().indexOf("z", 51) === -1, "nested rope: indexOf('z', 51) should be -1");
    assert(makeNestedRope().indexOf("z", 50) === 50, "nested rope: indexOf('z', 50) should be 50");
    assert(makeNestedRope().indexOf("z") === 50, "nested rope: indexOf('z') should be 50");
}

for (let i = 0; i < testLoopCount; i++) {
    testBasicRopeIndexOf();
    testRopeIndexOfWithPosition();
    testDeepRope();
    testFiberBoundary();
    testThreeFiberRope();
    testLastChar();
    testShortRope();
    testSubstringRopeRoot();
    testSubstringRopeFiber();
    testNestedRopeStartPosition();
}
