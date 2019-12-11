//@ runBigIntEnabled

function assert(v, e) {
    if (v !== e)
        throw new Error("Expected value: " + e + " but got: " + v)
}

function bigIntPropagation(a, b) {
    let c = a - b;
    return c - 0n;
}
noInline(bigIntPropagation);

for (let i = 0; i < 100000; i++) {
    let out = bigIntPropagation(0xffffffffffffffffffffffffffffffn, 0x1n);
    assert(out, 0xfffffffffffffffffffffffffffffen)
}

