assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testBitNot(x, z, message) {
    assert.sameValue(~x, z, message);
}

testBitNot(Object(1n), -2n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 1n;
    }
};
testBitNot(o, -2n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 1n;
    }
};
testBitNot(o, -2n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 1n;
    }
}
testBitNot(o, -2n, "ToPrimitive: toString");

// Test priority

function badAssertion() {
    throw new Error("This should never be called");
}

o = {
    [Symbol.toPrimitive]: function() {
        return 1n;
    },
    valueOf: badAssertion,
    toString: badAssertion
};
testBitNot(o, -2n, "ToPrimitive: @@toPrimitive and others throw");

o = {
    valueOf: function() {
        return 1n;
    },
    toString: badAssertion
};
testBitNot(o, -2n, "ToPrimitive: valueOf and toString throws");

