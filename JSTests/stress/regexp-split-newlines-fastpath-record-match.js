function shouldBe(actual, expected, name) {
    if (actual !== expected)
        throw new Error("FAIL " + name + ": got=" + JSON.stringify(actual) + " expected=" + JSON.stringify(expected));
}

function test() {
    /ZZZ/.exec("xxxZZZyyy");
    let r = "a\nb\nc".split(/\r\n?|\n/, 1);
    shouldBe(r.length, 1, "limit=1 length");
    shouldBe(r[0], "a", "limit=1 [0]");
    shouldBe(RegExp.lastMatch, "\n", "limit=1 lastMatch");
    shouldBe(RegExp.leftContext, "a", "limit=1 leftContext");
    shouldBe(RegExp.rightContext, "b\nc", "limit=1 rightContext");
    shouldBe(RegExp.input, "a\nb\nc", "limit=1 input");

    /ZZZ/.exec("xxxZZZyyy");
    r = "a\nb\nc".split(/\r\n?|\n/, 2);
    shouldBe(r.length, 2, "limit=2 length");
    shouldBe(r[0], "a", "limit=2 [0]");
    shouldBe(r[1], "b", "limit=2 [1]");
    shouldBe(RegExp.lastMatch, "\n", "limit=2 lastMatch");
    shouldBe(RegExp.leftContext, "a\nb", "limit=2 leftContext");
    shouldBe(RegExp.rightContext, "c", "limit=2 rightContext");

    /ZZZ/.exec("xxxZZZyyy");
    r = "a\r\nb\r\nc".split(/\r\n?|\n/, 1);
    shouldBe(r.length, 1, "CRLF limit=1 length");
    shouldBe(r[0], "a", "CRLF limit=1 [0]");
    shouldBe(RegExp.lastMatch, "\r\n", "CRLF limit=1 lastMatch");
    shouldBe(RegExp.leftContext, "a", "CRLF limit=1 leftContext");
    shouldBe(RegExp.rightContext, "b\r\nc", "CRLF limit=1 rightContext");

    /ZZZ/.exec("xxxZZZyyy");
    r = "a\rb".split(/\r\n?|\n/, 1);
    shouldBe(RegExp.lastMatch, "\r", "CR limit=1 lastMatch");
    shouldBe(RegExp.rightContext, "b", "CR limit=1 rightContext");

    /ZZZ/.exec("xxxZZZyyy");
    r = "a\nb\nc".split(/\r\n?|\n/);
    shouldBe(r.length, 3, "no-limit length");
    shouldBe(RegExp.lastMatch, "\n", "no-limit lastMatch");
    shouldBe(RegExp.leftContext, "a\nb", "no-limit leftContext");
    shouldBe(RegExp.rightContext, "c", "no-limit rightContext");

    /ZZZ/.exec("xxxZZZyyy");
    r = "abc".split(/\r\n?|\n/);
    shouldBe(r.length, 1, "no-match length");
    shouldBe(r[0], "abc", "no-match [0]");
    shouldBe(RegExp.lastMatch, "ZZZ", "no-match lastMatch preserved");
    shouldBe(RegExp.input, "xxxZZZyyy", "no-match input preserved");

    /ZZZ/.exec("xxxZZZyyy");
    r = "abc".split(/\r\n?|\n/, 1);
    shouldBe(r.length, 1, "no-match-limit length");
    shouldBe(RegExp.lastMatch, "ZZZ", "no-match-limit lastMatch preserved");

    /ZZZ/.exec("xxxZZZyyy");
    r = "\u3042\n\u3044\n\u3046".split(/\r\n?|\n/, 2);
    shouldBe(r.length, 2, "16bit limit=2 length");
    shouldBe(r[0], "\u3042", "16bit limit=2 [0]");
    shouldBe(r[1], "\u3044", "16bit limit=2 [1]");
    shouldBe(RegExp.lastMatch, "\n", "16bit limit=2 lastMatch");
    shouldBe(RegExp.leftContext, "\u3042\n\u3044", "16bit limit=2 leftContext");
    shouldBe(RegExp.rightContext, "\u3046", "16bit limit=2 rightContext");

    /ZZZ/.exec("xxxZZZyyy");
    "a\nb\nc".split(/\r\n?|\n/, 2);
    let fastLM = RegExp.lastMatch, fastLC = RegExp.leftContext, fastRC = RegExp.rightContext;

    /ZZZ/.exec("xxxZZZyyy");
    "a\nb\nc".split(/[\n]/, 2);
    shouldBe(fastLM, RegExp.lastMatch, "fast vs generic lastMatch");
    shouldBe(fastLC, RegExp.leftContext, "fast vs generic leftContext");
    shouldBe(fastRC, RegExp.rightContext, "fast vs generic rightContext");

    /ZZZ/.exec("xxxZZZyyy");
    r = "a\r\nb".split(/\n|\r\n?/, 1);
    shouldBe(r[0], "a", "reversed [0]");
    shouldBe(RegExp.lastMatch, "\r\n", "reversed lastMatch");
    shouldBe(RegExp.rightContext, "b", "reversed rightContext");
}

for (let i = 0; i < testLoopCount; ++i)
    test();
