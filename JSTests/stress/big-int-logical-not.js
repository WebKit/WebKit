function assert(a, e, n) {
    if (a !== e) {
        throw new Error("Bad logical negation for " + n);
    }
}

function logicalNot(a) {
    return !a;
}
noInline(logicalNot);

for (let i = 0; i < 100000; i++) {
    assert(logicalNot(10n), false, 10n);
    assert(logicalNot(1n), false, 1n);
    assert(logicalNot(0n), true, 0n);
    assert(logicalNot(-1n), false, -1n);
}

