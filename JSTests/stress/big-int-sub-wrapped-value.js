assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testSub(x, y, z, message) {
    assert.sameValue(x - y, z, message);
}

testSub(Object(2n), 1n, 1n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 2n;
    }
};
testSub(o, 1n, 1n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 2n;
    }
};
testSub(o, 1n, 1n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 2n;
    }
}
testSub(o, 1n, 1n, "ToPrimitive: toString");

