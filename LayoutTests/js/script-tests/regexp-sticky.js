description(
'Test for ES6 sticky flag regular expression processing'
);

function asString(o)
{
    if (o === null)
        return "<null>";

    return o.toString();
}

function testStickyExec(testDescription, re, str, beginLastIndex, expected)
{
    re.lastIndex = beginLastIndex;

    let failures = 0;

    for (let iter = 0; iter < expected.length; iter++) {
        let lastIndexStart = re.lastIndex;

        let result = re.exec(str);

        if (result != expected[iter]) {
            testFailed(testDescription + ", iteration " + iter + ", from lastIndex: " + lastIndexStart +
                       ", expected \"" + asString(expected[iter]) + "\", got \"" + asString(result) + "\"");
            failures++;
        }
    }

    if (failures)
        testFailed(testDescription + " - failed: " + failures + " tests");
    else
        testPassed(testDescription);
}

function testStickyMatch(testDescription, re, str, beginLastIndex, expected)
{
    re.lastIndex = beginLastIndex;

    let failures = 0;

    for (var iter = 0; iter < expected.length; iter++) {
        let lastIndexStart = re.lastIndex;

        let result = str.match(re);
        let correctResult = false;
        if (expected[iter] === null || result === null)
            correctResult = (expected[iter] === result);
        else if (result.length == expected[iter].length) {
            correctResult = true;
            for (let i = 0; i < result.length; i++) {
                if (result[i] != expected[iter][i])
                    correctResult = false;
            }
        }

        if (!correctResult) {
            testFailed(testDescription + ", iteration " + iter + ", from lastIndex: " + lastIndexStart +
                       ", expected \"" + asString(expected[iter]) + "\", got \"" + asString(result) + "\"");
            failures++;
        }
    }

    if (failures)
        testFailed(testDescription + " - failed: " + failures + " tests");
    else
        testPassed(testDescription);
}


testStickyExec("Repeating Pattern", new RegExp("abc", "y"), "abcabcabc", 0, ["abc", "abc", "abc", null]);
testStickyExec("Test lastIndex resets", /\d/y, "12345", 0, ["1", "2", "3", "4", "5", null, "1", "2", "3", "4", "5", null]);
testStickyExec("Ignore Case", new RegExp("test", "iy"), "TESTtestTest", 0, ["TEST", "test", "Test", null]);
testStickyExec("Alternates, differing lengths long to short", new RegExp("bb|a", "y"), "a", 0, ["a", null]);
testStickyExec("Alternates, differing lengths long to short with mutliple matches", new RegExp("abc|ab|a", "y"), "aababcaabcab", 0, ["a", "ab", "abc", "a", "abc", "ab", null]);
testStickyExec("Alternates, differing lengths, short to long", new RegExp("Dog |Cat |Mouse ", "y"), "Mouse Dog Cat ", 0, ["Mouse ", "Dog ", "Cat ", null]);
testStickyExec("BOL Anchored, starting at 0", /^X/y, "XXX", 0, ["X", null]);
testStickyExec("BOL Anchored, starting at 1", /^X/y, "XXX", 1, [null, "X", null]);
testStickyExec("EOL Anchored, not at EOL", /#$/y, "##", 0, [null]);
testStickyExec("EOL Anchored, at EOL", /#$/y, "##", 1, ["#", null]);
testStickyExec("Lookahead Assertion", /\d+(?=-)/y, "212-555-1212", 0, ["212", null]);
testStickyExec("Lookahead Negative Assertion", /\d+(?!\d)/y, "212-555-1212", 0, ["212", null]);
testStickyExec("Subpatterns - exec", /(\d+)(?:-|$)/y, "212-555-1212", 0, ["212-,212", "555-,555", "1212,1212"], null);
testStickyMatch("Subpatterns - match", /(\d+)(?:-|$)/y, "212-555-1212", 0, [["212-", "212"], ["555-", "555"], ["1212", "1212"]], null);
testStickyExec("Fixed Count", /\d{4}/y, "123456789", 0, ["1234", "5678", null]);
testStickyExec("Greedy", /\d*/y, "12345 67890", 0, ["12345", ""]);
testStickyMatch("Non-greedy", /\w+?./y, "abcdefg", 0, [["ab"], ["cd"], ["ef"], null]);
testStickyExec("Greedy/Non-greedy", /\s*(\d+)/y, "    1234  324512   74352", 0, ["    1234,1234", "  324512,324512", "   74352,74352", null]);
testStickyExec("Counted Range", /(\w+\s+){1,3}/y, "The quick brown fox jumped over the ", 0, ["The quick brown ,brown ", "fox jumped over ,over ", "the ,the ", null]);
testStickyMatch("Character Classes", /[0-9A-F]/iy, "fEEd123X", 0, [["f"], ["E"], ["E"], ["d"], ["1"], ["2"], ["3"], null]);
testStickyExec("Unmatched Greedy", /^\s*|\s*$/y, "ab", 1, [null]);
testStickyExec("Global Flag - exec", /\s*(\+|[0-9]+)\s*/gy, "3 + 4", 0, ["3 ,3", "+ ,+", "4,4", null]);
testStickyMatch("Global Flag - match", /\s*(\+|[0-9]+)\s*/gy, "3 + 4", 0, [["3 ", "+ ", "4"], ["3 ", "+ ", "4"]]);
testStickyMatch("Global Flag - Alternates, long to short", /x.|[\d]/gy, ".a", 0, [null]);
testStickyExec("Unicode Flag - Any Character", /./uy, "a@\u{10402}1\u202a\u{12345}", 0, ["a", "@", "\u{10402}", "1", "\u202a", "\u{12345}", null]);
testStickyMatch("Unicode & Ignore Case Flags", /(?:\u{118c0}|\u{10cb0}|\w):/iuy, "a:\u{118a0}:x:\u{10cb0}", 0, [["a:"], ["\u{118a0}:"], ["x:"], null]);
testStickyExec("Multiline", /(?:\w+ *)+(?:\n|$)/my, "Line One\nLine Two", 0, ["Line One\n", "Line Two", null]);
testStickyMatch("Multiline with BOL Anchor", /^\d*\s?/my, "13574\n295\n99", 0, [["13574\n"], ["295\n"], ["99"], null]);
testStickyExec("Multiline with EOL Anchor at start of Alternative", /.{2}|(?=$)./my, "\n", 0, [null]);

// Verify that String.search starts at 0 even with the sticky flag.
var re = new RegExp("\\d+\\s", "y");
shouldBe('"123 1234 ".search(re)', '0');
shouldBe('"123 1234 ".search(re)', '0');
// Verify that String.search doesn't advance past 0 with the sticky flag.
shouldBe('" 123 1234 ".search(re)', '-1');

re.lastIndex = 0;
shouldBeTrue('re.test("123 1234 ")');
shouldBe('re.lastIndex', '4');
shouldBeTrue('re.test("123 1234 ")');
shouldBe('re.lastIndex', '9');
shouldBeFalse('re.test("123 1234 ")');

