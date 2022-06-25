function assert(a, e) {
    if (a !== e) {
        throw new Error("Bad!");
    }
}

function logicalOr(a, b) {
    return a || b;
}
noInline(logicalOr);

for (let i = 0; i < 100000; i++) {
    assert(logicalOr(10n, "abc"), 10n);
    assert(logicalOr(1n, "abc"), 1n);
    assert(logicalOr(0n, "abc"), "abc");
    assert(logicalOr(-1n, "abc"), -1n);
}

