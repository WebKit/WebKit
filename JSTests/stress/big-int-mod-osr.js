let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntMod(x, y) {
    return x % y;
}
noInline(bigIntMod);

for (let i = 0; i < 10000; i++) {
    let r = bigIntMod(3n, 10n);
    assert.sameValue(r, 3n, 3n + " % " + 10n + " = " + r);
}

let r = bigIntMod(3, 10);
assert.sameValue(r, 3, 3 + " % " + 10 + " = " + r);

for (let i = 0; i < 10000; i++) {
    let r = bigIntMod(3n, 10n);
    assert.sameValue(r, 3n, 3n + " % " + 10n + " = " + r);
}

r = bigIntMod("3", "10");
assert.sameValue(r, 3, 3 + " % " + 10 + " = " + r);

