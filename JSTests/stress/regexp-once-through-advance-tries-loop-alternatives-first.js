function shouldBe(actual, expected, context) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("bad value: " + JSON.stringify(actual) + " expected: " + JSON.stringify(expected) + " for " + context);
}

function check(re, string, expected) {
    const result = re.exec(string);
    shouldBe(result === null ? null : Array.from(result), expected, re + " against " + JSON.stringify(string));
}

check(/a|^x|aa/, "caa", ["a"]);
check(/(?:^|[^g])x6|ar?/, "gax6", ["ax6"]);
check(/(^|[^g])hz+|w/, "uwhz", ["whz", "w"]);
check(/(?:^-|^\+)?d|dd/, "xdd", ["d"]);
shouldBe("caa xaa".replace(/a|^x|aa/g, "_"), "c__ x__", "replace /a|^x|aa/g");

check(/a(?<!q)|^x|aa/, "caa", ["a"]);
check(/foo(?<=foo)|^=|foobar/, " foobar", ["foo"]);
check(/(?<![a-z])b|^#|bb+/, "1bbb", ["b"]);

check(/^x|^y|a|aa/, "caa", ["a"]);
check(/^q|aa|^r|a/, "xcaa", ["aa"]);
check(/^a|^b/, "xab", null);
check(/^a(?<!b)|^b/, "xab", null);
check(/^a/, "ba", null);
check(/^a(?<!b)/, "ba", null);
