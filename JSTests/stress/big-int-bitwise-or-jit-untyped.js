let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntBitOrUntyped(a, b) {
    return a | b;
}
noInline(bigIntBitOrUntyped);

let o = { valueOf: () => 0b1111n };
for (let i = 0; i < 10000; i++) {
    let r = bigIntBitOrUntyped(o, 0b1010n);
    assert.sameValue(r, 0b1111n, 0b101n + " | " + 0b1010n + " = " + r);
}

