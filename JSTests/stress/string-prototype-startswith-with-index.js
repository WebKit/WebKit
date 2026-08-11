function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search, index) {
    return string.startsWith(search, index);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString(".............................................okokHellookok................................");
var search1 = makeString("Hello");
var search2 = makeString("okok");
var search3 = makeString("NotFound");

for (var i = 0; i < testLoopCount; ++i) {
    // Basic tests with index
    shouldBe(test(string, search1, 0), false);
    shouldBe(test(string, search1, 49), true);
    shouldBe(test(string, search1, 50), false);
    shouldBe(test(string, search1, string.length), false);

    shouldBe(test(string, search2, 0), false);
    shouldBe(test(string, search2, 45), true);
    shouldBe(test(string, search2, 46), false);

    shouldBe(test(string, search3, 0), false);
    shouldBe(test(string, search3, 20), false);

    // Negative index should be treated as 0
    shouldBe(test(string, makeString("."), -10), true);
    shouldBe(test(string, makeString("."), -1), true);
    shouldBe(test(string, search1, -1), false);

    // Edge cases
    shouldBe(test(makeString("Hello"), makeString("ello"), 1), true);
    shouldBe(test(makeString("Hello"), makeString("ello"), 0), false);
    shouldBe(test(makeString("Hello"), makeString("lo"), 3), true);
    shouldBe(test(makeString("Hello"), makeString(""), 3), true);
}
