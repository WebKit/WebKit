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

for (let i = 0; i < 10000; i++) {
    let r = bigIntRShift(0b10001n, 4n);
    assert.sameValue(r, 1n, 0b10001n + " >> " + 4n + " = " + r);
}

