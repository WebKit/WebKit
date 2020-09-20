assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testAdd(x, y, z, message) {
    assert.sameValue(x + y, z, message);
    assert.sameValue(y + x, z, message);
}

testAdd(Object(2n), 1n, 3n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 2n;
    }
};
testAdd(o, 1n, 3n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 2n;
    }
};
testAdd(o, 1n, 3n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 2n;
    }
}
testAdd(o, 1n, 3n, "ToPrimitive: toString");

