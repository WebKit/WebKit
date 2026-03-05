function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search, index) {
    return string.includes(search, index);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString(".............................................okokHellookok................................");
var searchH = makeString("H");
var searchX = makeString("X");
var searchDot = makeString(".");

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(string, searchH, 0), true);
    shouldBe(test(string, searchH, 20), true);
    shouldBe(test(string, searchH, 49), true);
    shouldBe(test(string, searchH, 50), false);
    shouldBe(test(string, searchH, string.length), false);

    shouldBe(test(string, searchX, 0), false);
    shouldBe(test(string, searchX, 20), false);

    shouldBe(test(string, searchDot, 0), true);
    shouldBe(test(string, searchDot, 44), true);
    shouldBe(test(string, searchDot, 57), true);  // After "okok"
    shouldBe(test(string, searchDot, string.length - 1), true);
    shouldBe(test(string, searchDot, string.length), false);

    shouldBe(test(string, searchH, -10), true);
    shouldBe(test(string, searchH, -1), true);
}
