//@ memoryHog!
//@ skip if $addressBits <= 32
//@ runDefault("--useConcurrentJIT=0")

function shouldThrowRangeError(func, ...args) {
    let error;
    try {
        func(...args);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof RangeError))
        throw new Error("bad error: " + error);
}

function test(kind, x) {
    let s = "ab";
    if (!kind)
        return s;
    s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s;
    s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s;
    s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s; s += s;
    switch (kind) {
    case 1:
        return `${s}${s}${s}`.length;
    case 2:
        return `${s}${s}${x}`.length;
    default:
        return `${x}${s}${s}`.length;
    }
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    shouldThrowRangeError(test, 1, "x");
    shouldThrowRangeError(test, 2, "x");
    shouldThrowRangeError(test, 3, "x");
    test(0, "x");
}
