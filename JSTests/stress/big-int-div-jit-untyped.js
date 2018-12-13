//@ runBigIntEnabled

let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntDiv(x, y) {
    return x / y;
}
noInline(bigIntDiv);

let o =  {valueOf: () => 10n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntDiv(30n, o);
    assert.sameValue(r, 3n, 30n + " / {valueOf: () => 10n} = " + r);
}

o2 =  {valueOf: () => 10000n};

for (let i = 0; i < 10000; i++) {
    let r = bigIntDiv(o2, o);
    assert.sameValue(r, 1000n, "{valueOf: () => 10000n} / {valueOf: () => 10n}  = " + r);
}

o = Object(10n);
let r = bigIntDiv(30n, o);
assert.sameValue(r, 3n, 30n + " / Object(10n) = " + r);

o2 = Object(3240n);
r = bigIntDiv(o2, o);
assert.sameValue(r, 324n, "Object(3240n) / Object(10n) = " + r);

