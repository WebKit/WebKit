function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " but got: " + a);
}

function bitAnd(a, b) {
    return a & b;
}
noInline(bitAnd);

var o = { valueOf: () => 0b1101 };

for (var i = 0; i < 10000; i++)
    assert(bitAnd(0b11, o), 0b1);

assert(numberOfDFGCompiles(bitAnd) <= 1, true);

function bitOr(a, b) {
    return a | b;
}
noInline(bitOr);

for (var i = 0; i < 10000; i++)
    assert(bitOr(0b11, o), 0b1111);

assert(numberOfDFGCompiles(bitOr) <= 1, true);

function bitXor(a, b) {
    return a ^ b;
}
noInline(bitXor);

for (var i = 0; i < 10000; i++)
    assert(bitXor(0b0011, o), 0b1110);

assert(numberOfDFGCompiles(bitXor) <= 1, true);

