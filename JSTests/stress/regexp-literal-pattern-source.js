function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldThrowSyntaxError(source) {
    let threw = false;
    try {
        eval(source);
    } catch (error) {
        threw = error instanceof SyntaxError;
    }
    shouldBe(threw, true);
}

shouldBe(/=abc/.source, "=abc");
shouldBe(/=/g.source, "=");
shouldBe(/a[/]b\/c/.source, "a[/]b\\/c");
shouldBe(eval("/=[/]\\//gi").source, "=[/]\\/");
shouldBe(eval("/=[/]\\//gi").flags, "gi");
shouldBe(eval("let x = 6; x /= 2; x"), 3);
shouldBe(eval("/é/").source, "é");
shouldBe(eval("'あ'; /=é/").source, "=é");
shouldBe(eval("'あ'; /=あ+/").source, "=あ+");
shouldBe(eval("'あ'; /あ[/]/u").test("あ/"), true);

shouldThrowSyntaxError("/abc");
shouldThrowSyntaxError("/=abc");
shouldThrowSyntaxError("/[/");
shouldThrowSyntaxError("/\\/");
shouldThrowSyntaxError("/abc\n/");
shouldThrowSyntaxError("'あ'; /=あ\n/");

for (let prefix of ["", "'あ'; "]) {
    shouldBe(eval(prefix + "/a/").flags, "");
    shouldBe(eval(prefix + "/a/g").flags, "g");
    shouldBe(eval(prefix + "/a/dgimsuy").flags, "dgimsuy");
    shouldBe(eval(prefix + "/a/gv ").flags, "gv");
    shouldBe(eval(prefix + "[/a/i][0]").flags, "i");
    shouldBe(eval(prefix + "/=a/y").source, "=a");
    shouldBe(eval(prefix + "/=a/y").flags, "y");

    shouldThrowSyntaxError(prefix + "/a/gg");
    shouldThrowSyntaxError(prefix + "/a/x");
    shouldThrowSyntaxError(prefix + "/a/\\u0067");
    shouldThrowSyntaxError(prefix + "/a/é");
    shouldThrowSyntaxError(prefix + "/a/gあ");
}
