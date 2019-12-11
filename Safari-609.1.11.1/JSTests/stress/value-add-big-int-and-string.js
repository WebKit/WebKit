//@ runBigIntEnabled

function assert(v, e) {
    if (v !== e)
        throw new Error("Expected value: " + e + " but got: " + v)
}

function bigIntOperations(a, b) {
    let c = a + b;
    return a + c;
}
noInline(bigIntOperations);

for (let i = 0; i < 100000; i++) {
    let out = bigIntOperations(0b1111n, "16");
    assert(out, "151516");
}

