function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected));
}

function test(s) {
    s.match(/l/g);
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i) {
    /seed/.test("seeded");
    test("hello");
    shouldBe(RegExp.lastMatch, "l");
    shouldBe(RegExp.input, "hello");
}

function testMulti(s) {
    s.match(/l+/g);
}
noInline(testMulti);

for (let i = 0; i < testLoopCount; ++i) {
    /seed/.test("seeded");
    testMulti("hello world");
    shouldBe(RegExp.lastMatch, "l");
    shouldBe(RegExp.leftContext, "hello wor");
    shouldBe(RegExp.rightContext, "d");
}

function testCapture(s) {
    s.match(/(l)(o)/g);
}
noInline(testCapture);

for (let i = 0; i < testLoopCount; ++i) {
    /(s)(e)/.test("seed");
    testCapture("hello foo");
    shouldBe(RegExp.$1, "l");
    shouldBe(RegExp.$2, "o");
}

function testNoMatch(s) {
    s.match(/xyz/g);
}
noInline(testNoMatch);

for (let i = 0; i < testLoopCount; ++i) {
    /seed/.test("seeded");
    testNoMatch("hello");
    shouldBe(RegExp.lastMatch, "seed");
}
