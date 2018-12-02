//@ runBigIntEnabled

assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testLeftShift(x, y, z, message) {
    assert.sameValue(x << y, z, message);
}

testLeftShift(Object(0b10n), 1n, 0b100n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 0b10n;
    }
};
testLeftShift(o, 0b01n, 0b100n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 0b10n;
    }
};
testLeftShift(o, 0b01n, 0b100n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 0b10n;
    }
}
testLeftShift(o, 0b01n, 0b100n, "ToPrimitive: toString");

