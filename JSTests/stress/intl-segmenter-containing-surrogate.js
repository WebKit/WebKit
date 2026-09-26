function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function checkContainingMatchesIteration(granularity, input) {
    let segments = new Intl.Segmenter("en", { granularity }).segment(input);
    let iterated = [...segments];
    for (let n = 0; n < input.length; ++n) {
        let hit = segments.containing(n);
        let expected = iterated.find(segment => n >= segment.index && n < segment.index + segment.segment.length);
        shouldBe(hit.index, expected.index);
        shouldBe(hit.segment, expected.segment);
    }
    shouldBe(segments.containing(-1), undefined);
    shouldBe(segments.containing(input.length), undefined);
}

// ' \u{1F600}' : the emoji is one grapheme whose lead surrogate is a boundary.
// containing() used to return the preceding segment for that lead.
{
    let input = " \u{1F600}";
    let segments = new Intl.Segmenter("en", { granularity: "grapheme" }).segment(input);
    let hit = segments.containing(1);
    shouldBe(hit.index, 1);
    shouldBe(hit.segment, "\u{1F600}");
    hit = segments.containing(2);
    shouldBe(hit.index, 1);
    shouldBe(hit.segment, "\u{1F600}");
}

for (let granularity of ["grapheme", "word", "sentence"]) {
    checkContainingMatchesIteration(granularity, " \u{1F600}");
    checkContainingMatchesIteration(granularity, "a\u{1F600}b");
    checkContainingMatchesIteration(granularity, "\u{1F600}\u{1F600}");
    checkContainingMatchesIteration(granularity, "\u{4E2D}\u{1F600}");
    checkContainingMatchesIteration(granularity, "e\u0301\u{1F600}");
    checkContainingMatchesIteration(granularity, "\u{1F468}\u200D\u{1F469}\u200D\u{1F467}");
}

// A supplementary character at index 0 still contains its trail surrogate.
{
    let segments = new Intl.Segmenter("ja", { granularity: "grapheme" }).segment("\u{20BB7}\u91CE\u5BB6");
    shouldBe(segments.containing(0).index, 0);
    shouldBe(segments.containing(0).segment, "\u{20BB7}");
    shouldBe(segments.containing(1).index, 0);
    shouldBe(segments.containing(1).segment, "\u{20BB7}");
}
