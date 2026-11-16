function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search) {
    return string.startsWith(search);
}
noInline(test);

function testWithIndex(string, search, index) {
    return string.startsWith(search, index);
}
noInline(testWithIndex);

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
var searchRope1 = makeRope("Hel", "lo");
var searchRope2 = makeRope("Hello", "World");
var searchRope3 = makeRope("AA", "AA");

for (var i = 0; i < testLoopCount; ++i) {
    // Basic rope tests
    shouldBe(test(rope1, "Hello"), true);
    shouldBe(test(rope1, "HelloWorld"), true);
    shouldBe(test(rope1, "World"), false);
    shouldBe(test(rope1, "HelloWorldExtra"), false);

    // Rope with rope search
    shouldBe(test(rope1, searchRope1), true);
    shouldBe(test(rope1, searchRope2), true);

    // Longer rope tests
    shouldBe(test(rope2, "HelloWorld"), true);
    shouldBe(test(rope2, "HelloWorldJavaScript"), true);
    shouldBe(test(rope2, "JavaScript"), false);

    // Long rope string tests
    shouldBe(test(longRope, longPart1), true);
    shouldBe(test(longRope, makeRope(longPart1, longPart2)), true);
    shouldBe(test(longRope, longPart2), false);
    shouldBe(test(longRope, searchRope3), true);

    // Rope with index
    shouldBe(testWithIndex(rope1, "World", 5), true);
    shouldBe(testWithIndex(rope1, "World", 0), false);
    shouldBe(testWithIndex(rope2, "JavaScript", 10), true);
    shouldBe(testWithIndex(longRope, longPart2, 40), true);
    shouldBe(testWithIndex(longRope, longPart3, 80), true);

    // Cross-boundary searches (search spans multiple rope segments)
    shouldBe(test(rope1, "loWo"), false);
    shouldBe(testWithIndex(rope1, "oWo", 4), true);
    shouldBe(testWithIndex(longRope, makeRope("A", "B"), 39), true);
}
