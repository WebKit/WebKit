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
var search1 = makeString("Hello");
var search2 = makeString("okHellooko");
var search3 = makeString("NotFound");

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(string, search1, 0), true);
    shouldBe(test(string, search1, 20), true);
    shouldBe(test(string, search1, 47), true);
    shouldBe(test(string, search1, 49), true);
    shouldBe(test(string, search1, 50), false);
    shouldBe(test(string, search1, string.length), false);

    shouldBe(test(string, search2, 0), true);
    shouldBe(test(string, search2, 20), true);
    shouldBe(test(string, search2, 47), true);
    shouldBe(test(string, search2, 48), false);
    shouldBe(test(string, search2, string.length), false);

    shouldBe(test(string, search3, 0), false);
    shouldBe(test(string, search3, 20), false);

    shouldBe(test(string, search1, -10), true);
    shouldBe(test(string, search1, -1), true);
}
