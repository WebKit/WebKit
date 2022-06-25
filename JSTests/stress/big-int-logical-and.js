function assert(a, e) {
    if (a !== e) {
        throw new Error("Bad!");
    }
}

function logicalAnd(a, b) {
    return a && b;
}
noInline(logicalAnd);

for (let i = 0; i < 100000; i++) {
    assert(logicalAnd(1n, 10n), 10n);
    assert(logicalAnd(1n, 1n), 1n);
    assert(logicalAnd(1n, 0n), 0n);
    assert(logicalAnd(1n, -1n), -1n);
}

