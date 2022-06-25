function assert(a) {
    if (!a)
        throw new Error("Bad assertion");
}

function lessThanOrEqualTest(a, b) {
    return a <= b;
}
noInline(lessThanOrEqualTest);

for (let i = 0; i < 100000; i++) {
    assert(lessThanOrEqualTest(3n, 4) === true);
}

for (let i = 0; i < 100000; i++) {
    assert(lessThanOrEqualTest(3n, 4n) === true);
}

for (let i = 0; i < 100000; i++) {
    assert(lessThanOrEqualTest(3n, "4") === true);
}

