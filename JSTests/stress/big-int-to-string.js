//@ runBigIntEnabled

function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

let v = 10n;
assert(v.toString() === "10");
assert(v.toString(2) === "1010");
assert(v.toString(3) === "101");
assert(v.toString(8) === "12");
assert(v.toString(16) === "a");
assert(v.toString(32) === "a");

// Invaid radix

function testInvalidRadix(radix) {
    try {
        v.toString(radix);
        assert(false);
    } catch(e) {
        assert(e instanceof RangeError);
    }
}

testInvalidRadix(-10);
testInvalidRadix(-1);
testInvalidRadix(0);
testInvalidRadix(1);
testInvalidRadix(37);
testInvalidRadix(4294967312);

