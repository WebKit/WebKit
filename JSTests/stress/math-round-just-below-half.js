function f(x) {
    return Math.round(x) | 0;
}

const unopt = f(0.49999999999999994); // should be 0

if (unopt !== 0)
    throw "Expected 0 for unopt, got " + unopt;

for (let i = 0; i < 200000; i++) {
    f(i * 0.1);
    if (f(0.49999999999999994) !== 0)
        throw "Expected 0 at iteration " + i;
}
