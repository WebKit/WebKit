function shouldBe(actual, expected) {
    actual = JSON.stringify(actual);
    expected = JSON.stringify(expected);
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function matchOf(re, string) {
    let match = re.exec(string);
    return match ? [match.index, ...match] : null;
}

shouldBe(matchOf(/(?!\u{1F600})./msu, "\u{1F600}"), null);
shouldBe(matchOf(/(?!\u{1F600})[\s\S]/mu, "\u{1F600}"), null);
shouldBe(matchOf(/(?!\u{1F600})[^]/mu, "\u{1F600}"), null);
shouldBe(matchOf(/(?!\u{1F600})./msv, "\u{1F600}"), null);
shouldBe(matchOf(/(?!\u{1F600})./msu, "\u{1F600}b"), [2, "b"]);
shouldBe(matchOf(/(?!\u{1F600})(.)/msu, "\u{1F600}b"), [2, "b", "b"]);
shouldBe(matchOf(/(?!\u{1F600}).{2}/msu, "\u{1F600}ab"), [2, "ab"]);
shouldBe(matchOf(/(?!\u{1F600}).+/msu, "\u{1F600}ab"), [2, "ab"]);
shouldBe(matchOf(/(?!\u{1F600}).+?b/msu, "\u{1F600}ab"), [2, "ab"]);
shouldBe(matchOf(/(?!\u{1F600})[\s\S]+/mu, "\u{1F600}ab"), [2, "ab"]);
shouldBe(matchOf(/(?!\u{1F600})[\s\S]{2}/mu, "\u{1F600}ab"), [2, "ab"]);
shouldBe(matchOf(/(?!\u{1F600})[\s\S]+?b/mu, "\u{1F600}ab"), [2, "ab"]);
shouldBe(matchOf(/(?!\u{1F600})./msu, "\u{1F600}\u{1F601}"), [2, "\u{1F601}"]);
shouldBe(matchOf(/(?!\u{1F600}).+/msu, "\u{1F600}\u{1F601}"), [2, "\u{1F601}"]);
shouldBe("\u{1F600}a\u{1F600}b".match(/(?!\u{1F600})./gmsu), ["a", "b"]);
shouldBe("\u{1F600}a\u{1F600}b".match(/(?!\u{1F600})[\s\S]/gmu), ["a", "b"]);
shouldBe(/(?!\u{1F600})./msu.test("\u{1F600}"), false);
shouldBe(/(?!\u{1F600})[\s\S]/mu.test("\u{1F600}"), false);
shouldBe(/(?!\u{1F600})./msu.test("\u{1F600}b"), true);
