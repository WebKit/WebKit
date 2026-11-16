function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function sumCodePoints(string) {
    var result = 0;
    for (var character of string)
        result += character.codePointAt(0);
    return result;
}
noInline(sumCodePoints);

function collect(string) {
    var result = [];
    for (var character of string)
        result.push(character);
    return result;
}
noInline(collect);

function firstChar(string) {
    for (var character of string)
        return character;
    return null;
}
noInline(firstChar);

var ascii = "Hello, World!";
var unicode = "a\u{20BB7}b野家\u{1F600}x";
var lonely = "a\uD842b\uDFB7c\uD842";

var expectedAscii = sumCodePoints(ascii);
var expectedUnicode = sumCodePoints(unicode);
var expectedLonely = sumCodePoints(lonely);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumCodePoints(ascii), expectedAscii);
    shouldBe(sumCodePoints(unicode), expectedUnicode);
    shouldBe(sumCodePoints(lonely), expectedLonely);
    shouldBe(firstChar("\u{20BB7}野"), "\u{20BB7}");
    shouldBe(firstChar(""), null);
}

shouldBe(collect(unicode).join("|"), "a|\u{20BB7}|b|野|家|\u{1F600}|x");
shouldBe(collect(lonely).join("|"), "a|\uD842|b|\uDFB7|c|\uD842");
shouldBe(collect("").length, 0);

// Mixed iterables on the same for-of site.
function countValues(iterable) {
    var count = 0;
    for (var value of iterable)
        count++;
    return count;
}
noInline(countValues);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(countValues("abc"), 3);
    shouldBe(countValues([1, 2, 3, 4]), 4);
    shouldBe(countValues(new Set([1, 2])), 2);
    shouldBe(countValues("\u{20BB7}野家"), 3);
}

// Exercise the fast path's 8-bit-inline vs 16-bit/rope/surrogate-slow-path split.
function makeRope(a, b) { return a + b + ""; }
noInline(makeRope);

for (var i = 0; i < testLoopCount; ++i) {
    // 8-bit high-latin1 (incl. NUL) take the inline single-character path.
    shouldBe(collect("caféÿ\x00").join("|"), "c|a|f|é|ÿ|\x00");
    // 16-bit non-surrogate goes through the slow path.
    shouldBe(collect("野家あ").join("|"), "野|家|あ");
    // Surrogate pair at the end / start, and two adjacent pairs.
    shouldBe(collect("ab\u{1F600}").join("|"), "a|b|\u{1F600}");
    shouldBe(collect("\u{20BB7}xy").join("|"), "\u{20BB7}|x|y");
    shouldBe(collect("\u{1F600}\u{1F601}").join("|"), "\u{1F600}|\u{1F601}");
    // Lone lead surrogate at the very end (no following code unit to read).
    shouldBe(collect("ab\uD842").join("|"), "a|b|\uD842");
    // Ropes resolve in the slow path.
    shouldBe(collect(makeRope("ab", "cd")).join("|"), "a|b|c|d");
    shouldBe(collect(makeRope("x", "\u{1F600}")).join("|"), "x|\u{1F600}");
    shouldBe(countValues(makeRope("hello ", "world")), 11);
}

// Manual next() after exhaustion must stay spec-correct ({ value: undefined, done: true }).
function exhaustManually() {
    var it = "a\u{1F600}"[Symbol.iterator]();
    shouldBe(it.next().value, "a");
    shouldBe(it.next().value, "\u{1F600}");
    var d = it.next();
    shouldBe(d.done, true);
    shouldBe(d.value, undefined);
    shouldBe(it.next().done, true);
}
noInline(exhaustManually);
for (var i = 0; i < testLoopCount; ++i)
    exhaustManually();

// Breaking the protocol must fall back to generic mode.
function joinChars(string) {
    var result = [];
    for (var character of string)
        result.push(character);
    return result.join("");
}
noInline(joinChars);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(joinChars("a\u{20BB7}b"), "a\u{20BB7}b");

var calledCustom = 0;
String.prototype[Symbol.iterator] = function* () {
    calledCustom++;
    yield "x";
    yield "y";
};
shouldBe(joinChars("a\u{20BB7}b"), "xy");
shouldBe(calledCustom, 1);
