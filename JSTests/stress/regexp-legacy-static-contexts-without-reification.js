function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function lexToken(line) {
    if (/^\s+/.test(line))
        line = RegExp.rightContext;
    if (/^[a-z]+/.test(line))
        return [RegExp.lastMatch, RegExp.leftContext, RegExp.rightContext];
    if (/^[0-9]+/.test(line))
        return [RegExp.lastMatch, RegExp.leftContext, RegExp.rightContext];
    if (/^,/.test(line))
        return [RegExp["$&"], RegExp.leftContext, RegExp.rightContext];
    return null;
}
noInline(lexToken);

function oneCharacter(input) {
    if (!/b/.test(input))
        return null;
    return [RegExp.lastMatch, RegExp.leftContext, RegExp.rightContext];
}
noInline(oneCharacter);

function globalOneCharacterLeftFirst(input) {
    if (!input.match(/b/g))
        return null;
    var leftContext = RegExp.leftContext;
    return [RegExp.lastMatch, leftContext, RegExp.rightContext];
}
noInline(globalOneCharacterLeftFirst);

function globalOneCharacterRightFirst(input) {
    if (!input.match(/b/g))
        return null;
    var rightContext = RegExp.rightContext;
    var leftContext = RegExp.leftContext;
    return [RegExp.lastMatch, leftContext, rightContext];
}
noInline(globalOneCharacterRightFirst);

function globalOneCharacterMatchFirst(input) {
    if (!input.match(/b/g))
        return null;
    return [RegExp["$&"], RegExp.leftContext, RegExp.rightContext];
}
noInline(globalOneCharacterMatchFirst);

function checkGlobalOneCharacter(input, expectedLeft, expectedRight) {
    for (var func of [globalOneCharacterLeftFirst, globalOneCharacterRightFirst, globalOneCharacterMatchFirst]) {
        var result = func(input);
        shouldBe(result[0], "b");
        shouldBe(result[1], expectedLeft);
        shouldBe(result[2], expectedRight);
    }
}

var base8 = "x".repeat(50) + "abcbd" + "y".repeat(50);
var base16 = "\u3042".repeat(50) + "abcbd" + "\u3044".repeat(50);

for (var i = 0; i < testLoopCount; ++i) {
    var result = lexToken("  print x");
    shouldBe(result[0], "print");
    shouldBe(result[1], "");
    shouldBe(result[2], " x");

    result = lexToken("1234 + 5");
    shouldBe(result[0], "1234");
    shouldBe(result[2], " + 5");

    result = lexToken(", y");
    shouldBe(result[0], ",");
    shouldBe(result[2], " y");

    shouldBe(lexToken("+"), null);
    shouldBe(RegExp.lastMatch, ",");

    result = oneCharacter("abcbd");
    shouldBe(result[0], "b");
    shouldBe(result[1], "a");
    shouldBe(result[2], "cbd");

    checkGlobalOneCharacter("abcbd", "abc", "d");
    checkGlobalOneCharacter("abcbd\u3042", "abc", "d\u3042");
    checkGlobalOneCharacter(base8.substring(10, 60), "x".repeat(40) + "abc", "d" + "y".repeat(5));
    checkGlobalOneCharacter(base8.substring(10, 70), "x".repeat(40) + "abc", "d" + "y".repeat(15));
    checkGlobalOneCharacter(base16.substring(10, 60), "\u3042".repeat(40) + "abc", "d" + "\u3044".repeat(5));
    checkGlobalOneCharacter(base16.substring(10, 70), "\u3042".repeat(40) + "abc", "d" + "\u3044".repeat(15));

    shouldBe("abcbd".match(/b/g).length, 2);
    shouldBe(RegExp.$1, "");
    shouldBe(RegExp.lastMatch, "b");
    shouldBe(RegExp.leftContext, "abc");
    shouldBe(RegExp.rightContext, "d");

    shouldBe(/(a)(b)?(c)/.test("xacy"), true);
    shouldBe(RegExp.lastMatch, "ac");
    shouldBe(RegExp.$1, "a");
    shouldBe(RegExp.$2, "");
    shouldBe(RegExp.lastParen, "c");
    shouldBe(RegExp.leftContext, "x");
    shouldBe(RegExp.rightContext, "y");
}
