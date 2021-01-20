function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert.sameValue = function (input, expected, message) {
    if (input !== expected)
        throw new Error(message);
}

function testDiv(x, y, z) {
    assert.sameValue(x / y, z, x + " / " + y + " = " + z);
}

let o = {
    [Symbol.toPrimitive]: function () { return 300000000000n; }
}

testDiv(500000000000438n, o, 1666n);

o.valueOf = function () {
    throw new Error("Should never execute it");
};

testDiv(700000000000438n, o, 2333n);

o.toString = function () {
    throw new Error("Should never execute it");
};

testDiv(700000000000438n, o, 2333n);

