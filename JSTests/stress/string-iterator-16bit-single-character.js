function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function collect(string) {
    var result = [];
    for (var character of string)
        result.push(character);
    return result;
}
noInline(collect);

function collectWithNext(string) {
    var result = [];
    var iterator = string[Symbol.iterator]();
    for (;;) {
        var entry = iterator.next();
        if (entry.done) {
            result.push(entry.value);
            break;
        }
        result.push(entry.value);
    }
    return result;
}
noInline(collectWithNext);

function countE(string) {
    var count = 0;
    for (var character of string) {
        if (character === 'e')
            count++;
    }
    return count;
}
noInline(countE);

function makeRope(a, b) {
    return a + b;
}
noInline(makeRope);

var cases = [
    ["see\u2014here", ["s", "e", "e", "\u2014", "h", "e", "r", "e"]],
    ["\u2014abc", ["\u2014", "a", "b", "c"]],
    ["abc\u2014", ["a", "b", "c", "\u2014"]],
    ["\u2014\x00\x7F\x80\xFE\xFF\u0100\u0101", ["\u2014", "\x00", "\x7F", "\x80", "\xFE", "\xFF", "\u0100", "\u0101"]],
    ["a\u{1F600}b", ["a", "\u{1F600}", "b"]],
    ["a\uD83Db", ["a", "\uD83D", "b"]],
    ["a\uDE00b", ["a", "\uDE00", "b"]],
    ["ab\uD83D", ["a", "b", "\uD83D"]],
    ["\uD83D\u{1F600}\uDE00e", ["\uD83D", "\u{1F600}", "\uDE00", "e"]],
    ["\u65E5e\u672Ce", ["\u65E5", "e", "\u672C", "e"]],
];

for (var i = 0; i < testLoopCount; ++i) {
    for (var [string, expected] of cases) {
        var actual = collect(string);
        shouldBe(actual.length, expected.length);
        for (var j = 0; j < expected.length; ++j)
            shouldBe(actual[j], expected[j]);

        var actualWithNext = collectWithNext(string);
        shouldBe(actualWithNext.length, expected.length + 1);
        for (var j = 0; j < expected.length; ++j)
            shouldBe(actualWithNext[j], expected[j]);
        shouldBe(actualWithNext[expected.length], undefined);
    }

    shouldBe(countE("see\u2014here"), 4);
    shouldBe(countE("see here"), 4);
    shouldBe(countE("\u65E5e\u{1F600}e\uD83D"), 2);
    shouldBe(countE(makeRope("see\u2014", "here here")), 6);
}
