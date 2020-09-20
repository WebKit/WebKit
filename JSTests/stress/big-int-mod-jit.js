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
    let r = bigIntMod(30n, 10n);
    assert.sameValue(r, 0n, 30n + " % " + 10n + " = " + r);
}

function bigIntModFolding(x, y) {
    let r = x % y;
    return -r;
}
noInline(bigIntModFolding);

for (let i = 0; i < 10000; i++) {
    let r = bigIntModFolding(10, 30);
    assert.sameValue(r, -10, "[Number] -(" + 10 + " % " + 30 + ") = " + r);
}

let r = bigIntModFolding(10n, 30n);
assert.sameValue(r, -10n, "[BigInt] -(" + 10n + " % " + 30n + ") = " + r);

