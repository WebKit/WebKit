assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testMul(x, y, z, message) {
    assert.sameValue(x * y, z, message);
    assert.sameValue(y * x, z, message);
}

testMul(Object(2n), 1n, 2n, "ToPrimitive: unbox object with internal slot");

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
testMul(o, 1n, 2n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 2n;
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testMul(o, 1n, 2n, "ToPrimitive: valueOf");

