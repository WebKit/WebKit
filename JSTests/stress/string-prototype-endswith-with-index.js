function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search, endPosition) {
    return string.endsWith(search, endPosition);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString("................................okokHellookok.............................................");
var search1 = makeString("Hello");
var search2 = makeString("okok");
var search3 = makeString("NotFound");

for (var i = 0; i < testLoopCount; ++i) {
    // Basic tests with endPosition
    shouldBe(test(string, search1, string.length), false);
    shouldBe(test(string, search1, 41), true);
    shouldBe(test(string, search1, 40), false);
    shouldBe(test(string, search1, 0), false);

    shouldBe(test(string, search2, string.length), false);
    shouldBe(test(string, search2, 36), true);
    shouldBe(test(string, search2, 35), false);
    shouldBe(test(string, search2, 45), true);

    shouldBe(test(string, search3, string.length), false);
    shouldBe(test(string, search3, 20), false);

    // Negative endPosition should be treated as 0
    shouldBe(test(string, makeString(""), -10), true);
    shouldBe(test(string, makeString(""), -1), true);
    shouldBe(test(string, makeString("."), -1), false);
    shouldBe(test(string, search1, -1), false);

    // Edge cases
    shouldBe(test(makeString("Hello"), makeString("Hell"), 4), true);
    shouldBe(test(makeString("Hello"), makeString("Hell"), 5), false);
    shouldBe(test(makeString("Hello"), makeString("lo"), 5), true);
    shouldBe(test(makeString("Hello"), makeString("lo"), 3), false);
    shouldBe(test(makeString("Hello"), makeString(""), 3), true);
}
