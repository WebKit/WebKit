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
    },
    valueOf: function () {
        throw new Error("Should never execute it");
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testAdd(o, 1n, 3n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 2n;
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testAdd(o, 1n, 3n, "ToPrimitive: valueOf");

