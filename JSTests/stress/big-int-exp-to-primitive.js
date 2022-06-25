function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

assert.sameValue = function (input, expected, message) {
    if (input !== expected)
        throw new Error(message);
}

function testExp(x, y, z) {
    assert.sameValue(x ** y, z, x + " * " + y + " = " + z);
}

let o = {
    [Symbol.toPrimitive]: function () { return 15n; }
}

testExp(15n, o, 437893890380859375n);

o.valueOf = function () {
    throw new Error("Should never execute it");
};

testExp(22n, o, 136880068015412051968n);

o.toString = function () {
    throw new Error("Should never execute it");
};

testExp(103n, o, 1557967416600764580522382952407n);

