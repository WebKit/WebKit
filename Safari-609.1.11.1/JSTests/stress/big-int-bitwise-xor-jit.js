//@ runBigIntEnabled

let assert = {
    sameValue: function(i, e) {
        if (i !== e)
            throw new Error(m);
    }
}

function bigIntBitXor(a, b) {
    return (a ^ b) ^ (a ^ 0b11n);

}
noInline(bigIntBitXor);

for (let i = 0; i < 10000; i++) {
    let r = bigIntBitXor(0b11n, 0b1010n);
    assert.sameValue(r, 0b1001n);
}

for (let i = 0; i < 10000; i++) {
    let r = bigIntBitXor(0xfffafafaf19281fefafeafebcn, 0b1010n);
    assert.sameValue(r, 0b1001n);
}

