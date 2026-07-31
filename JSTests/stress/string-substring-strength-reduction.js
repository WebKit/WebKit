function shouldBe(actual, expected) {
    if (!Object.is(actual, expected)) {
        throw new Error(`Bad value: ${actual}!`);
    }
}

// Each function uses literal constants so the strength-reduction phase
// can fold the StringSubstring node into a constant string.
const cases = [
    [() => "ABCDE".substring(0, 5), "ABCDE"],
    [() => "ABCDE".substring(1, 4), "BCD"],
    [() => "ABCDE".substring(2), "CDE"],
    [() => "ABCDE".substring(0), "ABCDE"],
    [() => "ABCDE".substring(2, 2), ""],
    [() => "ABCDE".substring(3, 3), ""],
    // Swap (start > end)
    [() => "ABCDE".substring(4, 1), "BCD"],
    [() => "ABCDE".substring(5, 0), "ABCDE"],
    // Out-of-range / negative clamping
    [() => "ABCDE".substring(-2, 2), "AB"],
    [() => "ABCDE".substring(2, -2), "AB"],
    [() => "ABCDE".substring(-5, -1), ""],
    [() => "ABCDE".substring(-100, 100), "ABCDE"],
    [() => "ABCDE".substring(2, 100), "CDE"],
    [() => "ABCDE".substring(100, 2), "CDE"],
    [() => "ABCDE".substring(5), ""],
    [() => "ABCDE".substring(100), ""],
    [() => "ABCDE".substring(100, 100), ""],
    // Single character
    [() => "ABCDE".substring(0, 1), "A"],
    [() => "ABCDE".substring(4, 5), "E"],
    [() => "ABCDE".substring(-1, 1), "A"],
    // Identity (start=0, end=length)
    [() => "ABCDE".substring(0, 5), "ABCDE"],
    // Empty source string
    [() => "".substring(0, 10), ""],
    [() => "".substring(-1, 1), ""],
    [() => "".substring(5, 1), ""],
    // Unicode (constant string with surrogates)
    [() => "𠮷野家".substring(0, 2), "𠮷"],
    [() => "𠮷野家".substring(2, 99), "野家"],
    [() => "𠮷野家".substring(-10, 10), "𠮷野家"],
];

for (const [fn, expected] of cases)
    noInline(fn);

for (let i = 0; i < testLoopCount; ++i) {
    for (const [fn, expected] of cases)
        shouldBe(fn(), expected);
}
