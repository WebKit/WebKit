function assert(a, e) {
    if (a !== e) {
        throw new Error("Bad!");
    }
}

function branchTest(a) {
    if (a)
        return a;
    else
        return false;
}
noInline(branchTest);

for (let i = 0; i < 100000; i++) {
    assert(branchTest(10n), 10n);
    assert(branchTest(1n), 1n);
    assert(branchTest(0n), false);
    assert(branchTest(-1n), -1n);
}

