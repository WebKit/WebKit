assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testEquals(x, y, z, message) {
    assert.sameValue(x == y, z, message);
    assert.sameValue(y == x, z, message);
}

testEquals(Object(2n), 1n, false, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 1n;
    }
};
testEquals(o, 1n, true, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 2n;
    }
};
testEquals(o, 2n, true, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 2n;
    }
}
testEquals(o, 1n, false, "ToPrimitive: toString");

