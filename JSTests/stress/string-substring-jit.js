function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`Bad value: ${actual}!`);
}

function referenceSubstring(string, start, end)
{
    var length = string.length;
    start = Math.min(Math.max(start, 0), length);
    end = end === undefined ? length : Math.min(Math.max(end, 0), length);
    if (start > end) {
        var swap = start;
        start = end;
        end = swap;
    }
    var result = "";
    for (var i = start; i < end; ++i)
        result += string[i];
    return result;
}

function substring(string, start, end)
{
    return string.substring(start, end);
}
noInline(substring);

function substringNoEnd(string, start)
{
    return string.substring(start);
}
noInline(substringNoEnd);

function makeRope(tail)
{
    var result = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    result += "0123456789";
    result += tail;
    return result;
}
noInline(makeRope);

var strings = ["", "A", "ABCDE", "ABCDEFGHIJKLMN", "\u3042\u3044\u3046\u3048\u304A", "\uD842\uDFB7\u91CE\u5BB6"];
var indices = [-100, -5, -1, 0, 1, 2, 3, 5, 13, 14, 100];

for (var i = 0; i < testLoopCount; ++i) {
    for (var string of strings) {
        for (var start of indices) {
            shouldBe(substringNoEnd(string, start), referenceSubstring(string, start, undefined));
            for (var end of indices)
                shouldBe(substring(string, start, end), referenceSubstring(string, start, end));
        }
    }

    shouldBe(substring(makeRope("XY"), 26, 30), "0123");
    shouldBe(substring(makeRope("XY"), 30, 26), "0123");
    shouldBe(substring(makeRope("XY"), 36, 37), "X");
    shouldBe(substring(makeRope("XY"), 36, 36), "");
    shouldBe(substring(makeRope("XY"), -1, 100), "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789XY");
    shouldBe(substringNoEnd(makeRope("XY"), 34), "89XY");
}
