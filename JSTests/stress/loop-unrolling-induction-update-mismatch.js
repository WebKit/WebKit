//@ requireOptions("--useConcurrentJIT=0")
function assert(actual, expected, name) {
    if (actual !== expected)
        throw new Error(name + ": bad actual=" + actual + " but expected=" + expected);
}

// The branch condition's update expression (i + 3) does not match the
// SetLocal that actually advances the induction variable (i = i + 1).
// LoopUnrolling must not derive the trip count from the condition's stride.
function overCount() {
    let i = 0;
    let x = 0;
    let t;
    do {
        x++;
    } while ((t = i + 3, i = i + 1, t < 9));
    return x;
}
noInline(overCount);

function underCount() {
    let i = 0;
    let x = 0;
    do {
        x++;
    } while (i + 1 < (i = i + 5, 3));
    return x;
}
noInline(underCount);

for (let k = 0; k < testLoopCount; k++) {
    assert(overCount(), 7, "overCount");
    assert(underCount(), 2, "underCount");
}
