assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testBitOr(x, y, z, message) {
    assert.sameValue(x | y, z, message);
    assert.sameValue(y | x, z, message);
}

testBitOr(Object(0b10n), 0b01n, 0b11n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 0b10n;
    }
};
testBitOr(o, 0b01n, 0b11n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 0b10n;
    }
};
testBitOr(o, 0b01n, 0b11n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 0b10n;
    }
}
testBitOr(o, 0b01n, 0b11n, "ToPrimitive: toString");

// BigInt with length > 1

testBitOr(Object(0b1111000000000000000000000000000000000000000000000000000000000000000n), 0b1000000000000000000000000000000000000000000000000000000000000001111n, 0b1111000000000000000000000000000000000000000000000000000000000001111n, "ToPrimitive: unbox object with internal slot");

o = {
    [Symbol.toPrimitive]: function() {
        return 0b1111000000000000000000000000000000000000000000000000000000000000000n;
    }
};
testBitOr(o, 0b1000000000000000000000000000000000000000000000000000000000000001111n, 0b1111000000000000000000000000000000000000000000000000000000000001111n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 0b1111000000000000000000000000000000000000000000000000000000000000000n;
    }
};
testBitOr(o, 0b1000000000000000000000000000000000000000000000000000000000000001111n, 0b1111000000000000000000000000000000000000000000000000000000000001111n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 0b1111000000000000000000000000000000000000000000000000000000000000000n;
    }
}
testBitOr(o, 0b1000000000000000000000000000000000000000000000000000000000000001111n, 0b1111000000000000000000000000000000000000000000000000000000000001111n, "ToPrimitive: toString");

