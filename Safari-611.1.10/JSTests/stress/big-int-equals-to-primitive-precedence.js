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
        return 2n;
    },
    valueOf: function () {
        throw new Error("Should never execute it");
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testEquals(o, 2n, true, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 2n;
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testEquals(o, 1n, false, "ToPrimitive: valueOf");

