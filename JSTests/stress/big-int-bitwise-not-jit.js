let assert = {
    sameValue: function(i, e, m) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntBitNot(a) {
    return ~(~a);
}
noInline(bigIntBitNot);

for (let i = 0; i < testLoopCount; i++) {
    let r = bigIntBitNot(3n);
    assert.sameValue(r, 3n, "~~" + 3n + " = " + r);
}

