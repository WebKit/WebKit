function assert(a, e) {
    if (a !== e)
        throw new Error("Expected " + e + " but got: " + a);
}

function iteration(a, b, r) {
    let acc = 0n;
    for (let i = 0n; i < r; i += 1n) {
        acc += a + b;
    }

    return acc;
}
noInline(iteration);

for (let i = 0; i < 10000; i++) {
    assert(iteration(1n, 2n, 100n), 300n)
}

