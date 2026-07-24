function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function charCodeAt(i) {
    return "abc".charCodeAt(i);
}

function foldedInRange() {
    return charCodeAt(1);
}
noInline(foldedInRange);

function foldedOutOfRange() {
    return charCodeAt(9);
}
noInline(foldedOutOfRange);

for (let i = 0; i < testLoopCount; i++)
    shouldBe(charCodeAt(i % 5), i % 5 < 3 ? 97 + (i % 5) : NaN);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(foldedInRange(), 98);
    shouldBe(foldedOutOfRange(), NaN);
}
