function shouldBe(actual, expected) {
    if (!Object.is(actual, expected)) {
        throw new Error(`Bad value: ${actual}!`);
    }
}

// Each function uses literal constants so the strength-reduction phase
// can fold the StringSubstr node into a constant string.
const cases = [
    [() => "ABCDE".substr(0, 5), "ABCDE"],
    [() => "ABCDE".substr(1, 3), "BCD"],
    [() => "ABCDE".substr(2), "CDE"],
    [() => "ABCDE".substr(0), "ABCDE"],
    [() => "ABCDE".substr(2, 0), ""],
    [() => "ABCDE".substr(2, -1), ""],
    [() => "ABCDE".substr(0, -100), ""],
    [() => "ABCDE".substr(2, 100), "CDE"],
    [() => "ABCDE".substr(-2), "DE"],
    [() => "ABCDE".substr(-2, 1), "D"],
    [() => "ABCDE".substr(-100, 3), "ABC"],
    [() => "ABCDE".substr(-100, 100), "ABCDE"],
    [() => "ABCDE".substr(5), ""],
    [() => "ABCDE".substr(100), ""],
    [() => "ABCDE".substr(100, 100), ""],
    [() => "ABCDE".substr(0, 1), "A"],
    [() => "ABCDE".substr(4, 1), "E"],
    [() => "ABCDE".substr(-1, 1), "E"],
    [() => "ABCDE".substr(-5, 1), "A"],
    // Identity (start=0 with span covering whole string)
    [() => "ABCDE".substr(0, 5), "ABCDE"],
    // Empty source string
    [() => "".substr(0, 10), ""],
    [() => "".substr(-1, 1), ""],
    [() => "".substr(5, 1), ""],
    // Unicode (constant string with surrogates)
    [() => "𠮷野家".substr(0, 2), "𠮷"],
    [() => "𠮷野家".substr(2, 99), "野家"],
    [() => "𠮷野家".substr(-10, 10), "𠮷野家"],
];

for (const [fn, expected] of cases)
    noInline(fn);

for (let i = 0; i < testLoopCount; ++i) {
    for (const [fn, expected] of cases)
        shouldBe(fn(), expected);
}
