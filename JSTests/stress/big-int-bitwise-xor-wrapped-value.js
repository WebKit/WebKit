assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testBitXor(x, y, z, message) {
    assert.sameValue(x ^ y, z, message);
    assert.sameValue(y ^ x, z, message);
}

testBitXor(Object(0b10n), 0b01n, 0b11n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 0b10n;
    }
};
testBitXor(o, 0b01n, 0b11n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 0b10n;
    }
};
testBitXor(o, 0b01n, 0b11n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 0b10n;
    }
}
testBitXor(o, 0b01n, 0b11n, "ToPrimitive: toString");

