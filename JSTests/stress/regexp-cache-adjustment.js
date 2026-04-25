function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function clearCache()
{
    "".match(/(?:)/g);
}

clearCache();
"foo bar bar bar bar baz".match(/bar/g);
shouldBe(RegExp.leftContext, "foo bar bar bar ");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "bar");

clearCache();
"foo bar bar bar bar baz".substring(1).match(/bar/g);
shouldBe(RegExp.leftContext, "oo bar bar bar ");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "bar");

clearCache();
"foo bar baz".match(/bar/g);
shouldBe(RegExp.leftContext, "foo ");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "bar");

clearCache();
"foo bar baz".match(/ba(r)/g);
shouldBe(RegExp.leftContext, "foo ");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "bar");

clearCache();
"foo xxxxxxxx baz".match(/x/g);
shouldBe(RegExp.leftContext, "foo xxxxxxx");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "x");

clearCache();
"foo xxxxxxxx baz".substring(1).match(/x/g);
shouldBe(RegExp.leftContext, "oo xxxxxxx");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "x");

clearCache();
"foo x baz".match(/x/g);
shouldBe(RegExp.leftContext, "foo ");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "x");

clearCache();
"foo x baz".match(/(x)/g);
shouldBe(RegExp.leftContext, "foo ");
shouldBe(RegExp.rightContext, " baz");
shouldBe(RegExp.lastMatch, "x");

{
    const veryLong = "foo bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar bar baz";
    clearCache();
    veryLong.substring(0, 10).match(/bar/g);
    shouldBe(RegExp.leftContext, "foo ");
    shouldBe(RegExp.rightContext, " ba");
    shouldBe(RegExp.lastMatch, "bar");

    clearCache();
    veryLong.substring(0, 20).match(/bar/g);
    shouldBe(RegExp.leftContext, "foo bar bar bar ");
    shouldBe(RegExp.rightContext, " ");
    shouldBe(RegExp.lastMatch, "bar");
}

{
    const veryLong = "foo xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx baz";
    clearCache();
    veryLong.substring(0, 10).match(/x/g);
    shouldBe(RegExp.leftContext, "foo xxxxx");
    shouldBe(RegExp.rightContext, "");
    shouldBe(RegExp.lastMatch, "x");

    clearCache();
    veryLong.substring(0, 20).match(/x/g);
    shouldBe(RegExp.leftContext, "foo xxxxxxxxxxxxxxx");
    shouldBe(RegExp.rightContext, "");
    shouldBe(RegExp.lastMatch, "x");
}
