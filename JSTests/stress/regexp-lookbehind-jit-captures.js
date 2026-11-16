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

shouldBe(matchOf(/(?<=(ab))\1/, "abab"), [2, "ab", "ab"]);
shouldBe(matchOf(/(?<=(a+))\1/, "xaaaa"), [2, "a", "a"]);
shouldBe(matchOf(/(?<=(a+?))\1/, "xaaaa"), [2, "a", "a"]);
shouldBe(matchOf(/(?<=(a)|(b))(?:\1|\2)c/, "bbc"), [1, "bc", undefined, "b"]);
shouldBe(matchOf(/(?<=(?<n>a)b)\k<n>/, "aba"), [2, "a", "a"]);
shouldBe(matchOf(/(?<=(?<n>ab)|(?<n>cd))x/, "cdx"), [2, "x", undefined, "cd"]);
shouldBe(/(?<=(?<n>ab)|(?<n>cd))x/.exec("cdx").groups.n, "cd");
shouldBe(matchOf(/(?<=(a+)(b+))c/d, "xaabbc"), [5, "c", "aa", "bb"]);
shouldBe(/(?<=(a+)(b+))c/d.exec("xaabbc").indices.slice(1), [[1, 3], [3, 5]]);
shouldBe(/(?<=(?<n>a+))c/d.exec("aac").indices.groups.n, [0, 2]);
shouldBe(/(?<=(a))\1/.test("aa"), true);
shouldBe(/(?<=(a))\1/.test("ab"), false);
shouldBe(matchOf(/(?:(?<=(a))b(\1))+/, "abaaba"), [1, "ba", "a", "a"]);
shouldBe(matchOf(/((?<=(\w))x)+/, "axbx"), [1, "x", "x", "a"]);
shouldBe(matchOf(/(?:(?<=(a)|(b))c){2}/, "acbc"), null);
shouldBe(matchOf(/(?:(?<=(a)|(b))c){2}/, "acac"), null);
shouldBe(matchOf(/(x(?<=(x))y)*z/, "xyxyz"), [0, "xyxyz", "xy", "x"]);
shouldBe(matchOf(/(?<=([ab])+)c/, "abc"), [2, "c", "a"]);
shouldBe("abcabd".replace(/(?<=(b))[cd]/g, "[$1]"), "ab[b]ab[b]");
shouldBe("aXbXc".split(/(?<=([abc]))X/), ["a", "a", "b", "b", "c"]);
