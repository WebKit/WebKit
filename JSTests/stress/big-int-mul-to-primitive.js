function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert.sameValue = function (input, expected, message) {
    if (input !== expected)
        throw new Error(message);
}

function testMul(x, y, z) {
    assert.sameValue(x * y, z, x + " * " + y + " = " + z);
    assert.sameValue(y * x, z, y + " * " + x + " = " + z);
}

let o = {
    [Symbol.toPrimitive]: function () { return 300000000000000n; }
}

testMul(500000000000438n, o, 150000000000131400000000000000n);

o.valueOf = function () {
    throw new Error("Should never execute it");
};

testMul(700000000000438n, o, 210000000000131400000000000000n);

o.toString = function () {
    throw new Error("Should never execute it");
};

testMul(700000000000438n, o, 210000000000131400000000000000n);

