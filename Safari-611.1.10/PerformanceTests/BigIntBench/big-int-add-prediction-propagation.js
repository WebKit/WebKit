function assert(v, e) {
    if (v !== e)
        throw new Error("Expected value: " + e + " but got: " + v)
}

function bigIntOperations(a, b) {
    let c = a + b;
    return c & 0b111111111n;
}
noInline(bigIntOperations);

for (let i = 0; i < 100000; i++) {
    let out = bigIntOperations(0xffffffffffffffffffffffffffffffn, 0x1n);
    assert(out, 0n)
}

for (let i = 0; i < 100000; i++) {
    let out = bigIntOperations(0b111111n, 0b1n);
    assert(out, 0b1000000n)
}

