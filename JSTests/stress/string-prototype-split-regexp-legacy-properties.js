function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var str = "prefix one two three four five six seven eight nine suffix";
var regexp = /(one) (two) (three) (four) (five) (six) (seven) (eight) (nine)/;
str.split(regexp);

for (var i = 0; i < 1e2; ++i) {
    str.split(regexp);
    shouldBe(RegExp.input, str);
    shouldBe(RegExp.lastMatch, "one two three four five six seven eight nine");
    shouldBe(RegExp.lastParen, "nine");
    shouldBe(RegExp.leftContext, "prefix ");
    shouldBe(RegExp.rightContext, " suffix");
    shouldBe(RegExp.$1, "one");
    shouldBe(RegExp.$2, "two");
    shouldBe(RegExp.$3, "three");
    shouldBe(RegExp.$4, "four");
    shouldBe(RegExp.$5, "five");
    shouldBe(RegExp.$6, "six");
    shouldBe(RegExp.$7, "seven");
    shouldBe(RegExp.$8, "eight");
    shouldBe(RegExp.$9, "nine");
    /test/.test("test");
}
