//@ runBigIntEnabled

let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntAddition(x, y) {
    return x - y;
}
noInline(bigIntAddition);

for (let i = 0; i < 10000; i++) {
    let r = bigIntAddition(3n, 10n);
    assert.sameValue(r, -7n, 3n + " - " + 10n + " = " + r);
}

