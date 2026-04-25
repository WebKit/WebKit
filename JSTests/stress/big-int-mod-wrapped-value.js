assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testDiv(x, y, z, message) {
    assert.sameValue(x % y, z, message);
}

testDiv(Object(33n), 10n, 3n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 33n;
    }
};
testDiv(o, 10n, 3n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 33n;
    }
};
testDiv(o, 10n, 3n, "ToPrimitive: valueOf");

o = {
    toString: function() {
        return 33n;
    }
}
testDiv(o, 10n, 3n, "ToPrimitive: toString");

