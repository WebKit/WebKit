let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntRShift(x, y) {
    return x >> y;
}
noInline(bigIntRShift);

let o =  {valueOf: () => 4n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntRShift(0b10001n, o);
    assert.sameValue(r, 1n, 0b10001n + " >> {valueOf: () => 4n} = " + r);
}

o2 =  {valueOf: () => 0b10000n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntRShift(o2, o);
    assert.sameValue(r, 1n, "{valueOf: () => 0b10000n} >> {valueOf: () => 4n}  = " + r);
}

o = Object(0b10n);
let r = bigIntRShift(0b11n, o);
assert.sameValue(r, 0n, 0b11n + " >> Object(0b10n) = " + r);

o2 = Object(0b1100001n);
r = bigIntRShift(o2, o);
assert.sameValue(r, 0b11000n, "Object(0b1100001n) * Object(10n) = " + r);

