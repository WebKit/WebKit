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
    let r = bigIntRShift(0b100111n, 2n);
    assert.sameValue(r, 0b1001n, 0b100111n + " >> " + 2n + " = " + r);
}

let r = bigIntRShift(3, 10);
assert.sameValue(r, 0, 3 + " >> " + 10 + " = " + r);

r = bigIntRShift("3", "10");
assert.sameValue(r, 0, 3 + " >> " + 10 + " = " + r);

