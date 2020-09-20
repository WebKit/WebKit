function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert.sameValue = function (input, expected, message) {
    if (input !== expected)
        throw new Error(message);
}

function testMod(x, y, z) {
    assert.sameValue(x % y, z, x + " % " + y + " = " + z);
}

let o = {
    [Symbol.toPrimitive]: function () { return 3000n; }
}

testMod(500000000000438n, o, 2438n);

o.valueOf = function () {
    throw new Error("Should never execute it");
};

testMod(700000000000438n, o, 1438n);

o.toString = function () {
    throw new Error("Should never execute it");
};

testMod(700000000000438n, o, 1438n);

