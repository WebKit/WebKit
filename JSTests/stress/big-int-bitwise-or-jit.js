//@ runBigIntEnabled

let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntBitOr(a, b) {
    return (a | b) | (a | 0b111n);
}
noInline(bigIntBitOr);

for (let i = 0; i < 10000; i++) {
    let r = bigIntBitOr(0b101n, 0b1010n);
    assert.sameValue(r, 0b1111n, 0b101n + " | " + 0b1010n + " = " + r);
}

