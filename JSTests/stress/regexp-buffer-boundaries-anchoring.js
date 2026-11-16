//@ requireOptions("--useRegExpBufferBoundaries=1")

// \A and \z participate in the same anchoring optimizations as non-multiline ^ and $
// (once-through alternatives and end-anchored fixed-size matching). Check that these
// patterns still produce exactly the same results as their lookaround equivalents at
// every start position, for every flag combination.

function shouldBe(actual, expected, message)
{
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

const inputs = [
    "", "a", "b", "c", "ab", "ba", "abc", "cba", "aab", "aaaa", "aaaaaaaaaaaab",
    "foo", "xfoo", "foox", "barfoo", "foobar", "afoob", "bcd", "abcd", "xbcd", "bcdx",
    "\n", "\r\n", "a\n", "\na", "a\nb", "b\na", "ab\nfoo", "foo\nbar", "foo\r\n", "\nabc",
    "abc\n", "b\nfoo\nfoo", "foo\nfoo\n", "a\r\n\n", "a b", "GET /", "PUT",
];

const boundaryToLookaround = new Map([
    ["\\A", "(?<![^])"],
    ["\\z", "(?![^])"],
    ["\\Z", "(?=(?:\\r\\n|\\n|\\r|\\u2028|\\u2029)?(?![^]))"],
]);

function checkEquivalent(source, flags)
{
    let reference = source;
    for (const [boundary, lookaround] of boundaryToLookaround)
        reference = reference.replaceAll(boundary, lookaround);
    const re = new RegExp(source, flags);
    const referenceRE = new RegExp(reference, flags);
    for (const input of inputs) {
        for (let lastIndex = 0; lastIndex <= input.length; ++lastIndex) {
            re.lastIndex = lastIndex;
            referenceRE.lastIndex = lastIndex;
            shouldBe(JSON.stringify(re.exec(input)), JSON.stringify(referenceRE.exec(input)), "/" + source + "/" + flags + " on " + JSON.stringify(input) + " at " + lastIndex);
            shouldBe(re.lastIndex, referenceRE.lastIndex, "lastIndex of /" + source + "/" + flags + " on " + JSON.stringify(input) + " at " + lastIndex);
        }
        shouldBe(JSON.stringify(input.split(re)), JSON.stringify(input.split(referenceRE)), "split /" + source + "/" + flags + " on " + JSON.stringify(input));
        if (flags.includes("g"))
            shouldBe(JSON.stringify(input.replace(re, "<$&>")), JSON.stringify(input.replace(referenceRE, "<$&>")), "replace /" + source + "/" + flags + " on " + JSON.stringify(input));
    }
}

const sources = [
    // \A anchored alternatives (once-through) mixed with looping ones.
    "\\A",
    "\\Afoo",
    "\\Afoo|bar",
    "bar|\\Afoo",
    "\\Afoo|foo",
    "\\Aa|\\Ab|c",
    "\\A(?:foo|bar)",
    "\\A(?:a+|b)*c",
    "\\A(a)(b)?",
    "(?:\\Aa|b)+",
    "(?:x|\\Ay)z",
    "\\Aa*b",
    "\\A[ab]{2,}",
    "(\\A)?x",
    "(\\Aa|b)\\1",
    "(?<n>\\Aa)b|\\k<n>c",
    "(?=\\Aa)a",
    "(?!\\Aa)[ab]",
    "(?<=\\Aa)b",
    "(?<!\\Ab)a",
    "(?:(?<=\\A.)b|c)",
    "\\Afoo|^bar",
    "^a|\\Ab|c$",
    "\\A(?:GET|PUT|POST) ",
    // \z end anchoring (fixed and variable size).
    "\\z",
    "foo\\z",
    "foo\\z|bar",
    "bar|foo\\z",
    "(?:ab|cd)\\z",
    "(?:a|bcd)\\z",
    "a\\zb",
    "(a)(b)\\z",
    "(a\\z)?b",
    "a.\\z",
    "[ab]{2,}\\z",
    ".*\\z",
    ".+\\z",
    "a(?=\\z)|b(?!\\z)",
    "(?:a\\z|b)+",
    "a$|b\\z|c",
    "(?:\\A|x)\\z",
    // Both anchors.
    "\\Aa\\z",
    "\\A(?:a|ab)\\z",
    "\\A|\\z",
    "\\A\\z",
    "\\A.*\\z",
    "\\A[\\s\\S]*\\z",
    "\\Afoo\\z|\\Abar",
    // \Z stays a variable-length assertion.
    "\\Z",
    "foo\\Z",
    "\\Aa\\Z",
    "a\\Z|b\\z",
];

for (const source of sources) {
    for (const flags of ["u", "v", "gu", "yu", "guy", "iu", "su", "mu", "smu", "gmu"])
        checkEquivalent(source, flags);
}
