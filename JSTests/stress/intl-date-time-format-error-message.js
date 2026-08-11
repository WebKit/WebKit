function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual);
}

let expected = 'RangeError: timeZoneName must be "short", "long", "shortOffset", "longOffset", "shortGeneric", or "longGeneric"';

try {
    new Intl.DateTimeFormat('en', { timeZoneName: 'shortGenric' }).resolvedOptions().timeZoneName;
} catch (error) {
    shouldBe(error.toString(), expected);
}
