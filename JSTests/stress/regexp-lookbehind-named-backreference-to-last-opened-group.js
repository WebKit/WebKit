function shouldBe(actual, expected) {
    actual = JSON.stringify(actual, (key, value) => value === undefined ? "<undefined>" : value);
    expected = JSON.stringify(expected, (key, value) => value === undefined ? "<undefined>" : value);
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function matchOf(re, string) {
    let match = re.exec(string);
    return match ? [match.index, ...match] : null;
}

// The referenced group is the last group opened before the lookbehind.
shouldBe(matchOf(/(?<n>.)..(?<=\k<n>.)/, "abc"), null);
shouldBe(matchOf(/(?<n>.)..(?<=\k<n>.)/, "aac"), [0, "aac", "a"]);
shouldBe(matchOf(/(?<n>.)(?<=\k<n>\k<n>)x/, "abx"), null);
shouldBe(matchOf(/(?<n>.)(?<=\k<n>\k<n>)x/, "bbx"), [1, "bx", "b"]);
shouldBe(matchOf(/(?<n>.)x(?<=\k<n>\k<n>x)b/, "axb"), null);
shouldBe(matchOf(/(?<n>.)x(?<=\k<n>\k<n>x)b/, "aaxb"), [1, "axb", "a"]);
shouldBe(matchOf(/(?<n>ab)c(?<=\k<n>abc)/, "ababc"), [2, "abc", "ab"]);
shouldBe(matchOf(/(?<n>ab)c(?<=\k<n>abc)/, "xxabc"), null);
shouldBe(matchOf(/(?<m>.)(?<n>.)..(?<=\k<n>.)/, "abcd"), null);
shouldBe(matchOf(/(?<m>.)(?<n>.)..(?<=\k<n>.)/, "abbd"), [0, "abbd", "a", "b"]);
shouldBe(matchOf(/(?<m>.)(?<n>.)..(?<=\k<m>.)/, "abcd"), null);
shouldBe(matchOf(/(?<m>.)(?<n>.)..(?<=\k<m>.)/, "abad"), [0, "abad", "a", "b"]);

shouldBe(matchOf(/(?<n>.)..(?<=\k<n>*.)/, "abc"), [0, "abc", "a"]);
shouldBe(matchOf(/(?<n>.)..(?<=\k<n>*.)/, "aaac"), [0, "aaa", "a"]);
shouldBe(matchOf(/(?<n>.)..(?<=\k<n>{2}.)/, "aaac"), [0, "aaa", "a"]);
shouldBe(matchOf(/(?<n>.)..(?<=\k<n>{2}.)/, "abac"), null);
shouldBe(matchOf(/(?<n>.)..(?<!\k<n>.)/, "abc"), [0, "abc", "a"]);
shouldBe(matchOf(/(?<n>.)..(?<!\k<n>.)/, "aac"), null);
shouldBe(matchOf(/(?<n>A)..(?<=\k<n>.)/i, "Abc"), null);
shouldBe(matchOf(/(?<n>A)..(?<=\k<n>.)/i, "Aac"), [0, "Aac", "A"]);
shouldBe(matchOf(/(?<n>\u{1F600})..(?<=\k<n>.)/u, "\u{1f600}bcd"), null);
shouldBe(matchOf(/(?<n>\u{1F600})..(?<=\k<n>.)/u, "\u{1f600}\u{1f600}cd"), [0, "\u{1f600}\u{1f600}c", "\u{1f600}"]);
shouldBe(matchOf(/(?<n>.)(?<=(?<=\k<n>).)x/, "abx"), null);
shouldBe(matchOf(/(?<n>.)(?<=(?<=\k<n>).)x/, "aax"), [1, "ax", "a"]);
shouldBe(matchOf(/(?<n>.)(?<=(?=\k<n>)..)x/, "abx"), null);
shouldBe(matchOf(/(?<n>.)(?<=(?=\k<n>)..)x/, "aax"), [1, "ax", "a"]);

// Duplicate named groups.
shouldBe(matchOf(/(?:(?<n>a)|(?<n>b))..(?<=\k<n>.)/, "bcd"), null);
shouldBe(matchOf(/(?:(?<n>a)|(?<n>b))..(?<=\k<n>.)/, "bbd"), [0, "bbd", undefined, "b"]);
shouldBe(matchOf(/(?:(?<n>a)|(?<n>b))..(?<=\k<n>.)/, "acd"), null);
shouldBe(matchOf(/(?:(?<n>a)|(?<n>b))..(?<=\k<n>.)/, "aad"), [0, "aad", "a", undefined]);

// The referenced group is inside the lookbehind and has not captured yet when the reference is matched.
shouldBe(matchOf(/(?<=(?<n>a)\k<n>)b/, "ab"), [1, "b", "a"]);
shouldBe(matchOf(/(?<=(?<n>a)\k<n>b)c/, "abc"), [2, "c", "a"]);
shouldBe(matchOf(/(?<=(?:(?<n>a)\k<n>)+)b/, "aab"), [2, "b", "a"]);
shouldBe(matchOf(/(?<n>x)|(?<n>a(?<=\k<n>a))/, "ba"), [1, "a", undefined, "a"]);
shouldBe(matchOf(/(?<n>x)|(?<=(?<n>\k<n>a))b/, "ab"), [1, "b", undefined, "a"]);
shouldBe(matchOf(/(?<n>x)|(?<=\k<n>(?<n>a))b/, "aab"), [2, "b", undefined, "a"]);
shouldBe(matchOf(/(?<n>x)|(?<=\k<n>(?<n>a))b/, "cab"), null);
