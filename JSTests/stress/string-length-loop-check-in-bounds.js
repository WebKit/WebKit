function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function sum(string) {
    let result = 0;
    for (let i = 0; i < string.length; ++i)
        result += string.charCodeAt(i);
    return result;
}
noInline(sum);

function sumThenLength(string) {
    let result = 0;
    for (let i = 0; i < string.length; ++i)
        result += string.charCodeAt(i);
    return result + string.length;
}
noInline(sumThenLength);

function sumCodePoints(string) {
    let result = 0;
    for (let i = 0; i < string.length; ++i)
        result += string.codePointAt(i);
    return result;
}
noInline(sumCodePoints);

function countWithAt(string, character) {
    let result = 0;
    for (let i = 0; i < string.length; ++i) {
        if (string.at(i) === character)
            ++result;
    }
    return result;
}
noInline(countWithAt);

function sumBackwards(string) {
    let result = 0;
    for (let i = string.length - 1; i >= 0; --i)
        result += string.charCodeAt(i);
    return result;
}
noInline(sumBackwards);

function sumBackwardsFromLength(string) {
    let result = 0;
    for (let i = string.length; i >= 0; --i)
        result += string.charCodeAt(i) | 0;
    return result;
}
noInline(sumBackwardsFromLength);

function sumFrom(string, start) {
    let result = 0;
    for (let i = start; i < string.length; ++i)
        result += string.charCodeAt(i);
    return result;
}
noInline(sumFrom);

function sumIncludingLength(string) {
    let result = 0;
    for (let i = 0; i <= string.length; ++i)
        result += string.charCodeAt(i) | 0;
    return result;
}
noInline(sumIncludingLength);

function sumOfOther(string, other) {
    let result = 0;
    for (let i = 0; i < string.length; ++i)
        result += other.charCodeAt(i);
    return result;
}
noInline(sumOfOther);

function sumReassigned(string, other) {
    let result = 0;
    for (let i = 0; i < string.length; ++i) {
        result += string.charCodeAt(i);
        if (i === 1)
            string = other;
    }
    return result;
}
noInline(sumReassigned);

function rope(a, b) {
    return a + b;
}
noInline(rope);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(sum("Hello"), 500);
    shouldBe(sum(""), 0);
    shouldBe(sum("あい"), 24710);
    shouldBe(sum(rope("Hello, this is ", "a rope string")), 2562);
    shouldBe(sumThenLength("Hello"), 505);
    shouldBe(sumThenLength(rope("Hello, this is ", "a rope string")), 2590);
    shouldBe(sumCodePoints("a\u{1F600}b"), 185539);
    shouldBe(sumCodePoints(rope("Hello, this is ", "a rope \u{1F600}")), 187243);
    shouldBe(countWithAt("banana", "a"), 3);
    shouldBe(countWithAt(rope("banana banana ", "banana banana"), "a"), 12);
    shouldBe(sumBackwards("Hello"), 500);
    shouldBe(sumBackwards(""), 0);
    shouldBe(sumBackwards(rope("Hello, this is ", "a rope string")), 2562);
    shouldBe(sumBackwardsFromLength("Hello"), 500);
    shouldBe(sumFrom("Hello", 1), 428);
    shouldBe(sumFrom("Hello", 5), 0);
    shouldBe(sumFrom("Hello", 6), 0);
    shouldBe(sumIncludingLength("Hello"), 500);
    shouldBe(sumOfOther("Hello", "World"), 520);
    shouldBe(sumReassigned("Hello", "Wor"), 287);
    shouldBe(sumReassigned("He", "World"), 495);
}

shouldBe(Number.isNaN(sumFrom("Hello", -1)), true);
shouldBe(Number.isNaN(sumOfOther("Hello", "Wor")), true);
shouldBe(sumOfOther("Wor", "Hello"), 281);
