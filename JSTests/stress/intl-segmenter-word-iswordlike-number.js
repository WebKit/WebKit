function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// Numeric word segments must have isWordLike === true.
// ICU returns rule status UBRK_WORD_NUMBER (== 100 == UBRK_WORD_NONE_LIMIT) for
// pure-number word segments. The "none" range is half-open,
// [UBRK_WORD_NONE, UBRK_WORD_NONE_LIMIT); treating 100 as "none" wrongly
// classifies numbers as non-word-like.

{
    let segmenter = new Intl.Segmenter("en", { granularity: "word" });

    // Pure number, standalone.
    {
        let input = "123";
        let results = [
            [ 0, 3, "123", true ],
        ];
        let cursor = 0;
        for (let { segment, index, isWordLike } of segmenter.segment(input)) {
            let result = results[cursor++];
            shouldBe(result[0], index);
            shouldBe(result[1], index + segment.length);
            shouldBe(result[2], segment);
            shouldBe(result[3], isWordLike);
        }
        shouldBe(cursor, results.length);
    }

    // Number between letter words.
    {
        let input = "abc 123 def";
        let results = [
            [ 0, 3, "abc", true ],
            [ 3, 4, " ", false ],
            [ 4, 7, "123", true ],
            [ 7, 8, " ", false ],
            [ 8, 11, "def", true ],
        ];
        let cursor = 0;
        for (let { segment, index, isWordLike } of segmenter.segment(input)) {
            let result = results[cursor++];
            shouldBe(result[0], index);
            shouldBe(result[1], index + segment.length);
            shouldBe(result[2], segment);
            shouldBe(result[3], isWordLike);
        }
        shouldBe(cursor, results.length);
    }

    // Mixed alphanumeric, decimal number, letter word.
    {
        let input = "v2 3.14 foo";
        let results = [
            [ 0, 2, "v2", true ],
            [ 2, 3, " ", false ],
            [ 3, 7, "3.14", true ],
            [ 7, 8, " ", false ],
            [ 8, 11, "foo", true ],
        ];
        let cursor = 0;
        for (let { segment, index, isWordLike } of segmenter.segment(input)) {
            let result = results[cursor++];
            shouldBe(result[0], index);
            shouldBe(result[1], index + segment.length);
            shouldBe(result[2], segment);
            shouldBe(result[3], isWordLike);
        }
        shouldBe(cursor, results.length);
    }

    // Single-digit number in a sentence.
    {
        let input = "The 2nd of 3 items";
        let results = [
            [ 0, 3, "The", true ],
            [ 3, 4, " ", false ],
            [ 4, 7, "2nd", true ],
            [ 7, 8, " ", false ],
            [ 8, 10, "of", true ],
            [ 10, 11, " ", false ],
            [ 11, 12, "3", true ],
            [ 12, 13, " ", false ],
            [ 13, 18, "items", true ],
        ];
        let cursor = 0;
        for (let { segment, index, isWordLike } of segmenter.segment(input)) {
            let result = results[cursor++];
            shouldBe(result[0], index);
            shouldBe(result[1], index + segment.length);
            shouldBe(result[2], segment);
            shouldBe(result[3], isWordLike);
        }
        shouldBe(cursor, results.length);
    }

    // %Segments.prototype%.containing must also report isWordLike === true for numbers.
    {
        let input = "abc 123 def";
        let segments = segmenter.segment(input);
        shouldBe(JSON.stringify(segments.containing(4)), `{"segment":"123","index":4,"input":"abc 123 def","isWordLike":true}`);
        shouldBe(segments.containing(5).isWordLike, true);
        shouldBe(segments.containing(0).isWordLike, true);
        shouldBe(segments.containing(3).isWordLike, false);
    }

    {
        let segments = segmenter.segment("123");
        shouldBe(segments.containing(0).isWordLike, true);
        shouldBe(segments.containing(2).isWordLike, true);
    }
}
