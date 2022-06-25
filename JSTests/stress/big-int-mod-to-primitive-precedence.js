assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

function testMod(x, y, z, message) {
    assert.sameValue(x % y, z, message);
}

testMod(Object(33n), 10n, 3n, "ToPrimitive: unbox object with internal slot");

let o = {
    [Symbol.toPrimitive]: function() {
        return 33n;
    },
    valueOf: function () {
        throw new Error("Should never execute it");
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testMod(o, 10n, 3n, "ToPrimitive: @@toPrimitive");

o = {
    valueOf: function() {
        return 33n;
    },
    toString: function () {
        throw new Error("Should never execute it");
    }
};
testMod(o, 10n, 3n, "ToPrimitive: valueOf");

