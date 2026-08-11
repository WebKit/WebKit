function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function shouldThrowRangeError(func) {
    let threw = false;
    try {
        func();
    } catch (error) {
        threw = true;
        if (!(error instanceof RangeError))
            throw new Error(`bad error: ${error}`);
    }
    if (!threw)
        throw new Error("did not throw");
}

const invalidCodePoints = [-1, -0.5, 0.5, NaN, Infinity, -Infinity, 0x110000, 4294967296, 4294967296 + 65, 1e100, -1e100, 65.5];
for (const codePoint of invalidCodePoints) {
    shouldThrowRangeError(() => String.fromCodePoint(codePoint));
    shouldThrowRangeError(() => String.fromCodePoint(65, codePoint));
}

shouldBe(String.fromCodePoint(0), "\0");
shouldBe(String.fromCodePoint(-0), "\0");
shouldBe(String.fromCodePoint(65), "A");
shouldBe(String.fromCodePoint(0x10FFFF).codePointAt(0), 0x10FFFF);
