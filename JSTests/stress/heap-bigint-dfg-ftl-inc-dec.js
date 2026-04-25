// Test HeapBigInt increment/decrement (Inc/Dec nodes)
function testInc(a) { return ++a; }
function testDec(a) { return --a; }
function testPostInc(a) {
    let old = a;
    a++;
    if (a !== old + 1n)
        throw new Error("postinc failed: " + a + " !== " + (old + 1n));
    return a;
}
function testPostDec(a) {
    let old = a;
    a--;
    if (a !== old - 1n)
        throw new Error("postdec failed");
    return a;
}

let big = 123456789012345678901234567890n;
for (let i = 0; i < 100000; i++) {
    if (testInc(big) !== 123456789012345678901234567891n)
        throw new Error("inc failed");
    if (testDec(big) !== 123456789012345678901234567889n)
        throw new Error("dec failed");
    testPostInc(big);
    testPostDec(big);
}

// Also test with small BigInts to exercise HeapBigInt path
for (let i = 0; i < 100000; i++) {
    if (testInc(0n) !== 1n)
        throw new Error("inc 0n failed");
    if (testDec(0n) !== -1n)
        throw new Error("dec 0n failed");
    if (testInc(-1n) !== 0n)
        throw new Error("inc -1n failed");
    if (testDec(1n) !== 0n)
        throw new Error("dec 1n failed");
}
