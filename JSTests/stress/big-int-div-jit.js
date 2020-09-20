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

for (let i = 0; i < 10000; i++) {
    let r = bigIntDiv(30n, 10n);
    assert.sameValue(r, 3n, 30n + " / " + 10n + " = " + r);
}

