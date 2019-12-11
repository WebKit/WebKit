//@ runBigIntEnabled

function assert(a, e) {
    if (a !== e) {
        throw new Error("Bad!");
    }
}

function logicalNot(a) {
    return !a;
}
noInline(logicalNot);

for (let i = 0; i < 100000; i++) {
    assert(logicalNot(10n), false);
    assert(logicalNot(1n), false);
    assert(logicalNot(0n), true);
    assert(logicalNot(-1n), false);
}

