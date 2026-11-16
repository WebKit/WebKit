function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function test(regExp, input) {
    return regExp.test(input);
}
noInline(test);

const constantRegExp = /abcdef\u{1F600}/u;
function testConstantUnicode(input) {
    return constantRegExp.test(input);
}
noInline(testConstantUnicode);

var valueOfCalls = 0;

function warmUp(regExps, input) {
    for (var regExp of regExps)
        regExp.lastIndex = 0;
    for (var i = 0; i < testLoopCount; ++i)
        test(regExps[i & 1], input);
}

// RegExpBuiltinExec performs ToLength(? Get(R, "lastIndex")) for every RegExp, global or not, so a
// throwing valueOf on lastIndex must propagate from RegExp.prototype.test even when the input is
// shorter than the pattern's minimum length.
function testThrowingValueOf(regExps, input) {
    warmUp(regExps, input);
    var before = valueOfCalls;
    regExps[0].lastIndex = { valueOf() { ++valueOfCalls; throw new Error("boom"); } };
    for (var i = 0; i < testLoopCount; ++i) {
        test(regExps[1], input);
        var thrown = null;
        try {
            test(regExps[0], input);
        } catch (error) {
            thrown = error;
        }
        shouldBe(thrown !== null, true);
        shouldBe(thrown.message, "boom");
    }
    shouldBe(valueOfCalls - before, testLoopCount);
}

function testCountingValueOf(regExps, input, expected) {
    warmUp(regExps, input);
    var before = valueOfCalls;
    regExps[0].lastIndex = { valueOf() { ++valueOfCalls; return 0; } };
    for (var i = 0; i < testLoopCount; ++i) {
        test(regExps[1], input);
        shouldBe(test(regExps[0], input), expected);
    }
    shouldBe(valueOfCalls - before, testLoopCount);
}

function testSymbolLastIndex(regExps, input) {
    warmUp(regExps, input);
    regExps[0].lastIndex = Symbol("lastIndex");
    for (var i = 0; i < testLoopCount; ++i) {
        test(regExps[1], input);
        var thrown = null;
        try {
            test(regExps[0], input);
        } catch (error) {
            thrown = error;
        }
        shouldBe(thrown instanceof TypeError, true);
    }
}

testThrowingValueOf([/abcdef/, /uvwxyz/], "abc");
testThrowingValueOf([/ab\u{1F600}/u, /cd\u{1F600}/u], "ab");
testCountingValueOf([/abcdef/, /uvwxyz/], "abc", false);
testSymbolLastIndex([/abcdef/, /uvwxyz/], "abc");

// Control: an input that is long enough still matches with an object lastIndex.
testCountingValueOf([/abc/, /uvw/], "xxabcxx", true);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(testConstantUnicode("abc"), false);
constantRegExp.lastIndex = { valueOf() { ++valueOfCalls; throw new Error("boom"); } };
{
    var before = valueOfCalls;
    for (var i = 0; i < testLoopCount; ++i) {
        var thrown = null;
        try {
            testConstantUnicode("abc");
        } catch (error) {
            thrown = error;
        }
        shouldBe(thrown !== null, true);
        shouldBe(thrown.message, "boom");
    }
    shouldBe(valueOfCalls - before, testLoopCount);
}
