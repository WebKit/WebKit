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

shouldBe(matchOf(new RegExp("(?<=a{1073741824}x)b"), "xb"), null);
shouldBe(matchOf(new RegExp("(?<=a{1073741824}x)b"), "\u0100xb"), null);
shouldBe(matchOf(new RegExp("(?<=xa{1073741824})b"), "xb"), null);
shouldBe(matchOf(new RegExp("(?<=(?:a{536870912}){2}x)b"), "xb"), null);
shouldBe(matchOf(new RegExp("(?<=a{2147483647})b"), "ab"), null);
shouldBe(matchOf(new RegExp("(?<=.{300}x)y"), "q".repeat(300) + "xy"), [301, "y"]);
shouldBe(matchOf(new RegExp("(?<=x.{300})y"), "x" + "q".repeat(300) + "y"), [301, "y"]);
shouldBe(matchOf(new RegExp("(?<=x.{300})y"), "z" + "q".repeat(300) + "y"), null);
shouldBe(matchOf(new RegExp("(?<=x.{300})y"), "\u0100x" + "q".repeat(300) + "y"), [302, "y"]);
