function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search) {
    return string.endsWith(search);
}
noInline(test);

function testWithEndPosition(string, search, endPosition) {
    return string.endsWith(search, endPosition);
}
noInline(testWithEndPosition);

// Create rope strings by concatenation
function makeRope(a, b) {
    return a + b;
}
noInline(makeRope);

function makeRope3(a, b, c) {
    return a + b + c;
}
noInline(makeRope3);

var part1 = "Hello";
var part2 = "World";
var part3 = "JavaScript";

// Basic rope strings
var rope1 = makeRope(part1, part2);
var rope2 = makeRope3(part1, part2, part3);

// Longer rope strings
var longPart1 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
var longPart2 = "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB";
var longPart3 = "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC";
var longRope = makeRope3(longPart1, longPart2, longPart3);

// Search strings as ropes
var searchRope1 = makeRope("Wor", "ld");
var searchRope2 = makeRope("Hello", "World");
var searchRope3 = makeRope("CC", "CC");

for (var i = 0; i < testLoopCount; ++i) {
    // Basic rope tests
    shouldBe(test(rope1, "World"), true);
    shouldBe(test(rope1, "HelloWorld"), true);
    shouldBe(test(rope1, "Hello"), false);
    shouldBe(test(rope1, "HelloWorldExtra"), false);

    // Rope with rope search
    shouldBe(test(rope1, searchRope1), true);
    shouldBe(test(rope1, searchRope2), true);

    // Longer rope tests
    shouldBe(test(rope2, "JavaScript"), true);
    shouldBe(test(rope2, "WorldJavaScript"), true);
    shouldBe(test(rope2, "HelloWorldJavaScript"), true);
    shouldBe(test(rope2, "World"), false);

    // Long rope string tests
    shouldBe(test(longRope, longPart3), true);
    shouldBe(test(longRope, makeRope(longPart2, longPart3)), true);
    shouldBe(test(longRope, longPart2), false);
    shouldBe(test(longRope, searchRope3), true);

    // Rope with endPosition
    shouldBe(testWithEndPosition(rope1, "Hello", 5), true);
    shouldBe(testWithEndPosition(rope1, "Hello", 10), false);
    shouldBe(testWithEndPosition(rope2, "HelloWorld", 10), true);
    shouldBe(testWithEndPosition(longRope, longPart1, 40), true);
    shouldBe(testWithEndPosition(longRope, longPart2, 80), true);

    // Cross-boundary searches (search spans multiple rope segments)
    shouldBe(test(rope1, "oWor"), false);
    shouldBe(testWithEndPosition(rope1, "Worl", 9), true);  // "HelloWorl".endsWith("Worl") = true
    shouldBe(testWithEndPosition(longRope, makeRope("A", "B"), 41), true); // ends with "AB" at position 41
}
