//@ runBigIntEnabled

var assert = {
    sameValue: function (input, expected) {
        if (input !== expected)
            throw new Error('Expected: ' + expected + ' but got: ' + input);
    }
};

var x = {y:1n}
assert.sameValue(x.y, 1n);

x = {y:{z:1n}};
assert.sameValue(x.y.z, 1n);

x = {y:-1212n}
assert.sameValue(x.y, -1212n);

x = {y:{z:-22312n}};
assert.sameValue(x.y.z, -22312n);

