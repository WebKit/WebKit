//@ runBigIntEnabled

let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntMul(x, y) {
    return x * y;
}
noInline(bigIntMul);

let o =  {valueOf: () => 10n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntMul(3n, o);
    assert.sameValue(r, 30n, 3n + " * {valueOf: () => 10n} = " + r);
}

o2 =  {valueOf: () => 10000n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntMul(o2, o);
    assert.sameValue(r, 100000n, "{valueOf: () => 10000n} * {valueOf: () => 10n}  = " + r);
}

o = Object(10n);
let r = bigIntMul(3n, o);
assert.sameValue(r, 30n, 3n + " * Object(10n) = " + r);

o2 = Object(3241n);
r = bigIntMul(o2, o);
assert.sameValue(r, 32410n, "Object(32410n) * Object(10n) = " + r);

