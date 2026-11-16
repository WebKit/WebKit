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

// Basic: rope includes with single character.
function testBasicRopeIncludes() {
    let rope = makeLongRope();
    assert(rope.includes("a") === true, "'a' should be found");
    assert(rope.includes("b") === true, "'b' should be found");
    assert(rope.includes("z") === false, "'z' should not be found");
}

// startPosition argument.
function testRopeIncludesWithPosition() {
    let rope = makeLongRope();
    assert(rope.includes("a", 100) === true, "'a' from 100 should be found");
    assert(rope.includes("a", 200) === false, "'a' from 200 should not be found");
    assert(rope.includes("b", 300) === true, "'b' from 300 should be found");
    assert(rope.includes("b", 400) === false, "'b' from 400 should not be found");
}

// Deep rope (multi-level concatenation).
function testDeepRope() {
    let s = "x".repeat(100);
    for (let i = 0; i < 10; i++)
        s = s + "y".repeat(30);
    // s is "x"*100 + "y"*300 = 400 chars, exceeds threshold
    assert(s.includes("x") === true, "'x' should be found");
    assert(s.includes("y") === true, "'y' should be found");
    assert(s.includes("z") === false, "'z' should not be found");
}

// Edge: character at fiber boundary.
function testFiberBoundary() {
    let a = "a".repeat(150);
    let b = "b" + "a".repeat(149);
    let rope = a + b; // 300 chars, 'b' is at position 150
    assert(rope.includes("b") === true, "'b' at fiber boundary should be found");
}

// Three-fiber rope.
function testThreeFiberRope() {
    let a = "a".repeat(100);
    let b = "b".repeat(100);
    let c = "c".repeat(100);
    let rope = a + b + c; // JSC may create a 3-fiber rope
    assert(rope.includes("a") === true);
    assert(rope.includes("b") === true);
    assert(rope.includes("c") === true);
    assert(rope.includes("c", 250) === true);
    assert(rope.includes("z") === false);
}

// Last character.
function testLastChar() {
    let a = "a".repeat(299);
    let rope = a + "z"; // 300 chars
    assert(rope.includes("z") === true);
}

// Ensure short ropes still work (they take the normal path).
function testShortRope() {
    let rope = "hello" + " world";
    assert(rope.includes("w") === true);
    assert(rope.includes("z") === false);
}

// Substring rope as root: slice() creates a substring rope.
function testSubstringRopeRoot() {
    let base = "a".repeat(200) + "b".repeat(200);
    // Force resolve so slice() creates a substring rope over a resolved base.
    base.charCodeAt(0);
    let sub = base.slice(100, 350); // 250 chars, substring rope
    assert(sub.includes("a") === true, "sub: 'a' should be found");
    assert(sub.includes("b") === true, "sub: 'b' should be found");
    assert(sub.includes("z") === false, "sub: 'z' should not be found");
    assert(sub.includes("b", 150) === true, "sub: 'b' from 150 should be found");
}

// Substring rope as fiber: concat a substring with another string.
function testSubstringRopeFiber() {
    let base = "x".repeat(400);
    base.charCodeAt(0);
    let sub = base.slice(0, 200); // substring rope, 200 chars of 'x'
    let rope = sub + "y".repeat(200); // fiber0=substring rope, fiber1=resolved
    assert(rope.includes("x") === true, "fiber sub: 'x' should be found");
    assert(rope.includes("y") === true, "fiber sub: 'y' should be found");
    assert(rope.includes("z") === false, "fiber sub: 'z' should not be found");
    assert(rope.includes("x", 199) === true, "fiber sub: 'x' at 199 should be found");
    assert(rope.includes("x", 200) === false, "fiber sub: no 'x' from 200");
}

// Nested rope with startPosition: ensure startPosition is not regressed
// when tryFindOneChar bails out on a non-substring rope fiber.
function makeNestedRope() {
    let partA = "a".repeat(50) + "z" + "a".repeat(449); // 500 chars, 'z' at index 50
    let partB = "a".repeat(500);                          // 500 chars
    let inner = partA + partB;                             // 1000 chars, rope
    let partC = "b".repeat(200);                           // 200 chars
    return inner + partC;                                  // 1200 chars, fiber0 = inner (rope)
}
function testNestedRopeStartPosition() {
    // Each assertion uses a fresh rope because the first includes call
    // resolves the rope, hiding the bug for subsequent calls.
    assert(makeNestedRope().includes("z", 100) === false, "nested rope: includes('z', 100) should be false");
    assert(makeNestedRope().includes("z", 51) === false, "nested rope: includes('z', 51) should be false");
    assert(makeNestedRope().includes("z", 50) === true, "nested rope: includes('z', 50) should be true");
    assert(makeNestedRope().includes("z") === true, "nested rope: includes('z') should be true");
}

for (let i = 0; i < testLoopCount; i++) {
    testBasicRopeIncludes();
    testRopeIncludesWithPosition();
    testDeepRope();
    testFiberBoundary();
    testThreeFiberRope();
    testLastChar();
    testShortRope();
    testSubstringRopeRoot();
    testSubstringRopeFiber();
    testNestedRopeStartPosition();
}
