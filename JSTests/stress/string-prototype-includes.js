function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function test(string, search) {
    return string.includes(search);
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
var search4 = makeString("okok");
var search5 = makeString("....");

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(string, search1), true);
    shouldBe(test(string, search2), true);
    shouldBe(test(string, search3), false);
    shouldBe(test(string, search4), true);
    shouldBe(test(string, search5), true);

    shouldBe(test(string, makeString("")), true);
    shouldBe(test(makeString(""), search1), false);
    shouldBe(test(makeString("Hello"), search1), true);
}
