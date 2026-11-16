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
var searchH = makeString("H");
var searchX = makeString("X");
var searchDot = makeString(".");
var searchO = makeString("o");
var searchK = makeString("k");

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(test(string, searchH), true);
    shouldBe(test(string, searchX), false);
    shouldBe(test(string, searchDot), true);
    shouldBe(test(string, searchO), true);
    shouldBe(test(string, searchK), true);

    shouldBe(test(makeString(""), searchH), false);
    shouldBe(test(makeString("H"), searchH), true);
    shouldBe(test(makeString("h"), searchH), false);
}
