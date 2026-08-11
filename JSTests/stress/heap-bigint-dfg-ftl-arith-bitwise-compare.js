function testAdd(a, b) { return a + b; }
function testSub(a, b) { return a - b; }
function testMul(a, b) { return a * b; }
function testDiv(a, b) { return a / b; }
function testMod(a, b) { return a % b; }
function testBitAnd(a, b) { return a & b; }
function testBitOr(a, b) { return a | b; }
function testBitXor(a, b) { return a ^ b; }
function testBitLShift(a, b) { return a << b; }
function testBitRShift(a, b) { return a >> b; }
function testBitNot(a) { return ~a; }
function testLess(a, b) { return a < b; }
function testLessEq(a, b) { return a <= b; }
function testGreater(a, b) { return a > b; }
function testGreaterEq(a, b) { return a >= b; }
function testEq(a, b) { return a == b; }
function testStrictEq(a, b) { return a === b; }

for (let i = 0; i < 100000; i++) {
    // Arithmetic
    if (testAdd(1n, 2n) !== 3n)
        throw new Error("add failed");
    if (testSub(5n, 3n) !== 2n)
        throw new Error("sub failed");
    if (testMul(3n, 4n) !== 12n)
        throw new Error("mul failed");
    if (testDiv(10n, 3n) !== 3n)
        throw new Error("div failed");
    if (testMod(10n, 3n) !== 1n)
        throw new Error("mod failed");

    // Bitwise
    if (testBitAnd(0xFFn, 0x0Fn) !== 0x0Fn)
        throw new Error("bitand failed");
    if (testBitOr(0xF0n, 0x0Fn) !== 0xFFn)
        throw new Error("bitor failed");
    if (testBitXor(0xFFn, 0x0Fn) !== 0xF0n)
        throw new Error("bitxor failed");
    if (testBitLShift(1n, 8n) !== 256n)
        throw new Error("lshift failed");
    if (testBitRShift(256n, 4n) !== 16n)
        throw new Error("rshift failed");
    if (testBitNot(0n) !== -1n)
        throw new Error("bitnot failed");

    // Comparisons
    if (testLess(1n, 2n) !== true)
        throw new Error("less failed");
    if (testLess(2n, 1n) !== false)
        throw new Error("less false case failed");
    if (testLessEq(2n, 2n) !== true)
        throw new Error("lesseq failed");
    if (testLessEq(3n, 2n) !== false)
        throw new Error("lesseq false case failed");
    if (testGreater(3n, 2n) !== true)
        throw new Error("greater failed");
    if (testGreater(1n, 2n) !== false)
        throw new Error("greater false case failed");
    if (testGreaterEq(2n, 2n) !== true)
        throw new Error("greatereq failed");
    if (testGreaterEq(1n, 2n) !== false)
        throw new Error("greatereq false case failed");
    if (testEq(2n, 2n) !== true)
        throw new Error("eq failed");
    if (testEq(1n, 2n) !== false)
        throw new Error("eq false case failed");
    if (testStrictEq(2n, 2n) !== true)
        throw new Error("stricteq failed");
    if (testStrictEq(1n, 2n) !== false)
        throw new Error("stricteq false case failed");
}

// Test with larger BigInts to exercise HeapBigInt path (not BigInt32)
let big1 = 123456789012345678901234567890n;
let big2 = 987654321098765432109876543210n;
for (let i = 0; i < 100000; i++) {
    if (testAdd(big1, big2) !== 1111111110111111111011111111100n)
        throw new Error("big add failed");
    if (testSub(big2, big1) !== 864197532086419753208641975320n)
        throw new Error("big sub failed");
    if (testLess(big1, big2) !== true)
        throw new Error("big less failed");
    if (testGreater(big2, big1) !== true)
        throw new Error("big greater failed");
    if (testEq(big1, big1) !== true)
        throw new Error("big eq failed");
    if (testStrictEq(big1, big1) !== true)
        throw new Error("big stricteq failed");
    if (testStrictEq(big1, big2) !== false)
        throw new Error("big stricteq false case failed");
    if (testBitAnd(big1, big2) !== (big1 & big2))
        throw new Error("big bitand failed");
    if (testBitOr(big1, big2) !== (big1 | big2))
        throw new Error("big bitor failed");
    if (testBitXor(big1, big2) !== (big1 ^ big2))
        throw new Error("big bitxor failed");
}
