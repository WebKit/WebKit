function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function greaterThanTest(a, b) {
    return a > b;
}
noInline(greaterThanTest);

for (let i = 0; i < 100000; i++) {
    assert(greaterThanTest(3n, 4) === false);
}

for (let i = 0; i < 100000; i++) {
    assert(greaterThanTest(3n, 4n) === false);
}

for (let i = 0; i < 100000; i++) {
    assert(greaterThanTest(3n, "4") === false);
}

