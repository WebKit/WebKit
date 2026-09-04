function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function test(regExp, input) {
    return regExp.test(input);
}
noInline(test);

function testUntyped(regExp, input) {
    return regExp.test(input);
}
noInline(testUntyped);

var toStringCalls = 0;
var shortObject = { toString() { ++toStringCalls; return "ab"; } };
var longObject = { toString() { ++toStringCalls; return "abc"; } };

function nonConstantString(string) {
    return string;
}
noInline(nonConstantString);

function rope(left, right) {
    return left + right;
}
noInline(rope);

var cases = [
    [/abc/, "", false],
    [/abc/, "ab", false],
    [/abc/, "abc", true],
    [/abc/, "xabc", true],
    [/^\/api\/users\/([^\/]+?)\/?$/i, "/", false],
    [/^\/api\/users\/([^\/]+?)\/?$/i, "/api/users/", false],
    [/^\/api\/users\/([^\/]+?)\/?$/i, "/API/users/1", true],
    [/a|bcd/, "", false],
    [/a|bcd/, "a", true],
    [/a|bcd/, "bc", false],
    [/a|bcd/, "bcd", true],
    [/(?<=abc)d/, "", false],
    [/(?<=abc)d/, "d", false],
    [/(?<=abc)d/, "abcd", true],
    [/(?<!abc)d/, "d", true],
    [/x*/, "", true],
    [/a{3}/, "aa", false],
    [/a{3}/, "aaa", true],
    [/(ab){2,}/, "ab", false],
    [/(ab){2,}/, "abab", true],
    [/ab+/, "a", false],
    [/ab+/, "ab", true],
    [/(?:ab|c)+d/, "d", false],
    [/(?:ab|c)+d/, "cd", true],
    [/(a)\1/, "a", false],
    [/(a)\1/, "aa", true],
    [/(a)?\1b/, "b", true],
    [/(?=ab)a/, "a", false],
    [/(?=ab)a/, "ab", true],
    [/\bab\b/, "a", false],
    [/\bab\b/, "ab", true],
    [/^ab$/m, "a\nab", true],
    [/\u{1F600}y/u, "ay", false],
    [/\u{1F600}y/u, "\u{1F600}", false],
    [/\u{1F600}y/u, "\u{1F600}y", true],
    [/😀/, "\uD83D", false],
    [/😀/, "\u{1F600}", true],
    [/\u{1F600}/u, "\uD83D", false],
    [/\u{1F600}/u, "\u{1F600}", true],
    [/é/, "é", true],
    [/é/, "あ", false],
    [/ab/i, "あ", false],
    [/ab/i, "あAB", true],
];

for (var i = 0; i < testLoopCount; ++i) {
    for (var [regExp, input, expected] of cases) {
        shouldBe(test(regExp, nonConstantString(input)), expected);
        shouldBe(testUntyped(regExp, nonConstantString(input)), expected);
    }

    shouldBe(test(/abc/, rope("a", "b")), false);
    shouldBe(test(/abc/, rope("ab", "c")), true);
    shouldBe(test(/abcd/, rope("ab", "c")), false);

    shouldBe(testUntyped(/abc/, 12), false);
    shouldBe(testUntyped(/12/, 12345), true);
    shouldBe(testUntyped(/abc/, null), false);
    shouldBe(testUntyped(/null/, null), true);
    toStringCalls = 0;
    shouldBe(testUntyped(/abc/, shortObject), false);
    shouldBe(testUntyped(/abc/, longObject), true);
    shouldBe(toStringCalls, 2);

    var global = /abc/g;
    global.lastIndex = 3;
    shouldBe(test(global, "xxabc"), false);
    shouldBe(global.lastIndex, 0);
    global.lastIndex = 2;
    shouldBe(test(global, "xxabc"), true);
    shouldBe(global.lastIndex, 5);

    var sticky = /abc/y;
    sticky.lastIndex = 2;
    shouldBe(test(sticky, "xxabc"), true);
    shouldBe(sticky.lastIndex, 5);
    sticky.lastIndex = 3;
    shouldBe(test(sticky, "xxabc"), false);
    shouldBe(sticky.lastIndex, 0);

    var recompiled = /abcdef/;
    shouldBe(test(recompiled, "abc"), false);
    recompiled.compile("a");
    shouldBe(test(recompiled, "abc"), true);
    recompiled.compile("abcdefgh");
    shouldBe(test(recompiled, "abc"), false);
}

function testConstantUnicode(input) {
    return /abc/u.test(input);
}
noInline(testConstantUnicode);

function testConstantGlobal(input) {
    return /abc/g.test(input);
}
noInline(testConstantGlobal);

function testConstantZeroMinimum(input) {
    return /(?:abc)*d?/u.test(input);
}
noInline(testConstantZeroMinimum);

var hoisted = /abcdef/u;
function testHoisted(input) {
    return hoisted.test(input);
}
noInline(testHoisted);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(testConstantUnicode(nonConstantString("ab")), false);
    shouldBe(testConstantUnicode(nonConstantString("abc")), true);
    shouldBe(testConstantUnicode(rope("a", "b")), false);
    shouldBe(testConstantUnicode(rope("ab", "c")), true);
    shouldBe(testConstantGlobal(nonConstantString("ab")), false);
    shouldBe(testConstantGlobal(nonConstantString("abc")), true);
    shouldBe(testConstantZeroMinimum(nonConstantString("")), true);
    shouldBe(testHoisted(nonConstantString("abc")), false);
    shouldBe(testHoisted(nonConstantString("abcdef")), true);
}
hoisted.compile("a", "u");
shouldBe(testHoisted(nonConstantString("abc")), true);
hoisted.compile("abcdefgh", "u");
shouldBe(testHoisted(nonConstantString("abc")), false);

shouldBe(/abc/.exec("ab"), null);
shouldBe(/abc/.exec("abc")[0], "abc");
shouldBe("ab".match(/abc/), null);
shouldBe("ab".match(/abc/g), null);
shouldBe("ab".replace(/abc/, "x"), "ab");
shouldBe("ab".replace(/abc/g, "x"), "ab");
shouldBe("ab".search(/abc/), -1);
shouldBe("ab".split(/abc/).length, 1);
shouldBe("abcab".split(/abc/).length, 2);
shouldBe([..."ab".matchAll(/abc/g)].length, 0);

function shouldThrow(func, errorMessage) {
    var error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("not thrown");
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

var compilationFailure = "SyntaxError: Invalid regular expression: pattern exceeds string length limits";

function testUncompilable(input) {
    return /(?:a{2147483648}|)a{2147483648}|bc/.test(input);
}
noInline(testUncompilable);

for (var i = 0; i < testLoopCount; ++i) {
    shouldThrow(() => testUncompilable(nonConstantString("x")), compilationFailure);
    shouldThrow(() => test(RegExp(`(?:a{2147483648}|)a{2147483648}|b${i}`), nonConstantString("x")), compilationFailure);
}
