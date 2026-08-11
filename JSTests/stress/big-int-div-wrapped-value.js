assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testDiv(x, y, z, message) {
    assert.sameValue(x / y, z, message);
}

testDiv(Object(2n), 1n, 2n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 2n;
    }
};
testDiv(o, 1n, 2n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 2n;
    }
};
testDiv(o, 1n, 2n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 2n;
    }
}
testDiv(o, 1n, 2n, "ToPrimitive: toString");

o = {
    valueOf: function() {
        return 2n;
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testDiv(o, 1n, 2n, "ToPrimitive: valueOf");

