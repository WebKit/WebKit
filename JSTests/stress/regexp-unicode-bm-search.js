function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// 1. Basic BMP Unicode patterns
{
    let re = /abc/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("xyzabcxyz"), true);
        shouldBe(re.test("xyzxyz"), false);
        shouldBe(re.test("abc"), true);
        shouldBe(re.test("ab"), false);
    }
}

// 2. non-BMP characters in input string (surrogate pair boundary)
{
    let re = /abc/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}abc"), true);
        shouldBe(re.test("abc\u{10000}"), true);
        shouldBe(re.test("\u{10000}\u{10001}abc"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}"), false);
    }
}

// 3. non-BMP characters before and after match
{
    let re = /abcd/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}abcd"), true);
        shouldBe(re.test("abcd\u{10000}"), true);
        shouldBe(re.test("\u{10000}\u{10000}abcd"), true);
        shouldBe(re.test("\u{10000}abcd\u{10000}"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}abcd\u{10003}"), true);
        shouldBe(re.test("\u{10000}abc"), false);
    }
}

// 4. Dangling surrogates in input
{
    let re = /abc/u;
    for (let i = 0; i < 100; i++) {
        // High surrogate without low surrogate
        shouldBe(re.test("\uD800abc"), true);
        shouldBe(re.test("abc\uD800"), true);
        shouldBe(re.test("\uD800\uD800abc"), true);
        shouldBe(re.test("\uD800\uD800\uD800"), false);
        // Low surrogate without high surrogate
        shouldBe(re.test("\uDC00abc"), true);
        shouldBe(re.test("abc\uDC00"), true);
        shouldBe(re.test("\uDC00\uDC00abc"), true);
        shouldBe(re.test("\uDC00\uDC00\uDC00"), false);
    }
}

// 5. Hash collision: DEL (0x7F) has same hash bucket as errorCodePoint (-1 & 127 = 127)
{
    let re = /\x7Fabc/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\x7Fabc"), true);
        shouldBe(re.test("xabc"), false);
        shouldBe(re.test("\u{10000}\x7Fabc"), true);
        shouldBe(re.test("\uD800\x7Fabc"), true);
    }
}

// 6. Large stride (long pattern) with non-BMP input
{
    let re = /abcdefgh/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}\u{10003}abcdefgh"), true);
        shouldBe(re.test("abcdefgh\u{10000}\u{10001}\u{10002}\u{10003}"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}\u{10003}abcdefg"), false);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}\u{10003}"), false);
    }
}

// 7. ignoreCase + unicode
{
    let re = /abc/iu;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("ABC"), true);
        shouldBe(re.test("Abc"), true);
        shouldBe(re.test("\u{10000}ABC"), true);
        shouldBe(re.test("\u{10000}ABD"), false);
        shouldBe(re.test("\uD800ABC"), true);
    }
}

// 8. Character class + unicode
{
    let re = /[a-z]abc/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("xabc"), true);
        shouldBe(re.test("1abc"), false);
        shouldBe(re.test("\u{10000}xabc"), true);
        shouldBe(re.test("\u{10000}1abc"), false);
    }
}

// 9. Alternation + unicode
{
    let re = /abcde|fghij/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("abcde"), true);
        shouldBe(re.test("fghij"), true);
        shouldBe(re.test("abcdf"), false);
        shouldBe(re.test("\u{10000}abcde"), true);
        shouldBe(re.test("\u{10000}fghij"), true);
        shouldBe(re.test("\u{10000}\u{10001}xxxxx"), false);
    }
}

// 10. v-flag (UnicodeSets)
{
    let re = /abc/v;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("xyzabcxyz"), true);
        shouldBe(re.test("xyzxyz"), false);
        shouldBe(re.test("\u{10000}abc"), true);
        shouldBe(re.test("\u{10000}\u{10001}"), false);
    }
}

// 11. CJK + unicode (non-ASCII BMP pattern)
{
    let re = /\u4ECA\u65E5/u; // 今日
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u4ECA\u65E5"), true);
        shouldBe(re.test("xx\u4ECA\u65E5xx"), true);
        shouldBe(re.test("\u{10000}\u4ECA\u65E5"), true);
        shouldBe(re.test("\u4ECA"), false);
        shouldBe(re.test("\u65E5"), false);
    }
}

// 12. BoyerMooreFastCandidates path (<=2 candidates use direct comparison instead of bitmap)
{
    let re = /aaa|baa/u; // First char: [ab] (2 candidates), second/third: [a] (1 candidate)
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("aaa"), true);
        shouldBe(re.test("baa"), true);
        shouldBe(re.test("caa"), false);
        shouldBe(re.test("\u{10000}aaa"), true);
        shouldBe(re.test("\u{10000}baa"), true);
        shouldBe(re.test("\u{10000}caa"), false);
    }
}

// 13. Input is entirely non-BMP (no match expected)
{
    let re = /abc/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}"), false);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}\u{10003}\u{10004}\u{10005}"), false);
        shouldBe(re.test("\u{1F600}\u{1F601}\u{1F602}"), false);
    }
}

// 14. RegExp.exec() result verification (index and captures)
{
    let re = /abc/u;
    for (let i = 0; i < 100; i++) {
        let result = re.exec("xyzabc");
        shouldBe(result !== null, true);
        shouldBe(result[0], "abc");
        shouldBe(result.index, 3);

        // After non-BMP prefix: "\u{10000}" is 2 UTF-16 code units
        result = re.exec("\u{10000}abc");
        shouldBe(result !== null, true);
        shouldBe(result[0], "abc");
        shouldBe(result.index, 2);

        // After two non-BMP chars: 4 UTF-16 code units
        result = re.exec("\u{10000}\u{10001}abc");
        shouldBe(result !== null, true);
        shouldBe(result[0], "abc");
        shouldBe(result.index, 4);

        result = re.exec("xyz");
        shouldBe(result, null);
    }
}

// 14b. exec() with captures and unicode
{
    let re = /(ab)(cd)/u;
    for (let i = 0; i < 100; i++) {
        let result = re.exec("\u{10000}abcd");
        shouldBe(result !== null, true);
        shouldBe(result[0], "abcd");
        shouldBe(result[1], "ab");
        shouldBe(result[2], "cd");
        shouldBe(result.index, 2);
    }
}

// 15. Mixed: long pattern with alternation, ignoreCase, and non-BMP input
{
    let re = /abcdef|ghijkl/iu;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}\u{10001}ABCDEF"), true);
        shouldBe(re.test("\u{10000}\u{10001}GHIJKL"), true);
        shouldBe(re.test("\u{10000}\u{10001}abcdef"), true);
        shouldBe(re.test("\u{10000}\u{10001}xxxxxx"), false);
    }
}

// ============================================================
// Edge cases for readCharacterRaw (raw code unit read in BM loop)
//
// readCharacterRaw loads a raw UTF-16 code unit without surrogate decoding.
// tryReadUnicodeChar would decode surrogate pairs or return errorCodePoint (-1).
// These tests exercise scenarios where the two diverge, to verify that the
// BM prefilter produces no false negatives with raw reads.
// ============================================================

// 16. Lone lead surrogate as a BM pattern character
//     Pattern contains \uD800 which enters BM info (U16_LENGTH(0xD800)==1).
//     readCharacterRaw reads raw 0xD800 from input → matches the fast path candidate.
{
    let re = /\uD800abcd/u;
    for (let i = 0; i < 100; i++) {
        // Lone lead surrogate in input → should match
        shouldBe(re.test("\uD800abcd"), true);
        shouldBe(re.test("xx\uD800abcd"), true);
        // Surrogate pair \uD800\uDC00 = U+10000, NOT a lone \uD800 → should not match
        shouldBe(re.test("\uD800\uDC00abcd"), false);
        // Another surrogate pair prefix then lone surrogate match
        shouldBe(re.test("\u{10000}\uD800abcd"), true);
        shouldBe(re.test("xxxxx"), false);
    }
}

// 17. Lone trail surrogate as a BM pattern character
//     \uDC00 enters BM info. readCharacterRaw reads raw 0xDC00.
//     tryReadUnicodeChar would return 0xDC00 for a lone trail, or errorCodePoint if
//     preceded by a lead surrogate (part of a valid pair).
{
    let re = /\uDC00abcd/u;
    for (let i = 0; i < 100; i++) {
        // Lone trail surrogate at start → should match
        shouldBe(re.test("\uDC00abcd"), true);
        shouldBe(re.test("xx\uDC00abcd"), true);
        // Trail surrogate preceded by lead = valid pair → NOT a lone trail → no match
        shouldBe(re.test("\uD800\uDC00abcd"), false);
        shouldBe(re.test("xxxxx"), false);
    }
}

// 18. BM fast path with 2 surrogate candidates (direct comparison, not bitmap)
//     Two lead surrogates as fast path candidates, exercising readCharacterRaw
//     direct equality check against raw code units.
{
    let re = /\uD800aaa|\uD801aaa/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\uD800aaa"), true);
        shouldBe(re.test("\uD801aaa"), true);
        shouldBe(re.test("\uD802aaa"), false);
        // \uD800\uDC00 is a surrogate pair → not lone \uD800 → no match
        shouldBe(re.test("\uD800\uDC00aaa"), false);
        // Non-BMP prefix then lone surrogate match
        shouldBe(re.test("\u{10000}\uD800aaa"), true);
    }
}

// 19. BM stride landing on high surrogate of a valid pair
//     readCharacterRaw reads raw high surrogate (e.g. 0xD800).
//     tryReadUnicodeChar would decode to U+10000+.
//     Both should skip (no BMP pattern char matches surrogates), but hash buckets differ:
//     0xD800 & 127 = 0 vs U+10000 & 127 = 0 (happen to be same, but other pairs differ).
{
    let re = /abcd/u;
    for (let i = 0; i < 100; i++) {
        // Surrogate pairs before match: stride may land on high or low surrogate
        shouldBe(re.test("\u{10000}abcd"), true);
        shouldBe(re.test("\u{10001}abcd"), true);  // 0xD800 & 127=0 vs 0xD800 & 127=0
        shouldBe(re.test("\u{10041}abcd"), true);   // high surrogate 0xD801, 0xD801 & 127=1
        shouldBe(re.test("\u{10080}abcd"), true);   // 0xD802, & 127 = 2
        shouldBe(re.test("\u{1007F}abcd"), true);   // high surrogate 0xD801, trail 0xDC7F
        shouldBe(re.test("\u{10000}\u{10000}abcd"), true);
        shouldBe(re.test("\u{10000}\u{10000}\u{10000}abcd"), true);
        shouldBe(re.test("\u{10000}\u{10000}\u{10000}abcx"), false);
    }
}

// 20. BM stride landing on low surrogate of a valid pair
//     readCharacterRaw reads raw trail surrogate (e.g. 0xDC00).
//     tryReadUnicodeChar would return errorCodePoint (-1).
//     Hash: 0xDC00 & 127 = 0 vs (-1) & 127 = 127 — different buckets!
//     Both should result in "not a candidate" → skip → no false negative.
{
    let re = /abcde/u;
    for (let i = 0; i < 100; i++) {
        // Single non-BMP char = 2 code units. With stride 5, various landing positions.
        shouldBe(re.test("\u{10000}abcde"), true);
        shouldBe(re.test("x\u{10000}abcde"), true);
        shouldBe(re.test("xx\u{10000}abcde"), true);
        shouldBe(re.test("xxx\u{10000}abcde"), true);
        shouldBe(re.test("\u{10000}\u{10000}abcde"), true);
        shouldBe(re.test("\u{10000}\u{10000}\u{10000}abcde"), true);
        // No match
        shouldBe(re.test("\u{10000}\u{10000}\u{10000}abcdx"), false);
        shouldBe(re.test("\u{10000}\u{10000}\u{10000}\u{10000}\u{10000}"), false);
    }
}

// 21. Hash collision: raw surrogate hashes to bucket 0, same as NUL (0x00)
//     With tryReadUnicodeChar, a pair's high surrogate would decode to U+10000+ (bucket varies)
//     or errorCodePoint (bucket 127). With readCharacterRaw, 0xD800 → bucket 0.
//     Pattern has NUL char (bucket 0) → readCharacterRaw gets a false positive at
//     surrogate positions. This is safe but exercises the collision path.
{
    let re = /\0abcd/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\0abcd"), true);
        // Surrogate pair before match: BM may false-positive on high surrogate (bucket 0 = NUL)
        // but real matching correctly rejects it, then finds \0abcd
        shouldBe(re.test("\u{10000}\0abcd"), true);
        shouldBe(re.test("\u{10000}\u{10000}\0abcd"), true);
        shouldBe(re.test("\u{10000}abcd"), false);
        shouldBe(re.test("xabcd"), false);
    }
}

// 22. Hash collision: raw trail surrogate (0xDC00, bucket 0) vs pattern with \x80 (bucket 0)
//     errorCodePoint (-1) would hash to bucket 127, but raw 0xDC00 hashes to bucket 0.
{
    let re = /\x80abcd/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\x80abcd"), true);
        // Trail surrogates in input hash to bucket 0 = same as \x80
        // BM false-positive is safe, real matching rejects
        shouldBe(re.test("\u{10000}\x80abcd"), true);
        shouldBe(re.test("\u{10000}\u{10000}\x80abcd"), true);
        shouldBe(re.test("\u{10000}xabcd"), false);
    }
}

// 23. exec() with lone surrogate pattern char — verify index correctness
{
    let re = /\uD800abc/u;
    for (let i = 0; i < 100; i++) {
        let result = re.exec("xx\uD800abc");
        shouldBe(result !== null, true);
        shouldBe(result[0], "\uD800abc");
        shouldBe(result.index, 2);

        // After a surrogate pair (2 code units) then lone surrogate match
        result = re.exec("\u{10000}\uD800abc");
        shouldBe(result !== null, true);
        shouldBe(result[0], "\uD800abc");
        shouldBe(result.index, 2);

        // Should NOT match: \uD800\uDC00 is a pair, not lone \uD800
        result = re.exec("\uD800\uDC00abc");
        shouldBe(result, null);
    }
}

// 24. Alternation with surrogates: one branch has lone surrogate, other has BMP
//     BM collects candidates from both branches.
{
    let re = /\uD800xyz|abcxyz/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\uD800xyz"), true);
        shouldBe(re.test("abcxyz"), true);
        shouldBe(re.test("\u{10000}\uD800xyz"), true);
        shouldBe(re.test("\u{10000}abcxyz"), true);
        // \uD800\uDC00 = pair, not lone \uD800
        shouldBe(re.test("\uD800\uDC00xyz"), false);
        shouldBe(re.test("xxxxxx"), false);
    }
}

// 25. v-flag with lone surrogates in input
{
    let re = /abcde/v;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\uD800abcde"), true);
        shouldBe(re.test("\uDC00abcde"), true);
        shouldBe(re.test("\uD800\uDC00abcde"), true);
        shouldBe(re.test("\uD800\uD800abcde"), true);
        shouldBe(re.test("\uDC00\uDC00abcde"), true);
        shouldBe(re.test("\uD800\uDC00\uD800\uDC00abcde"), true);
        shouldBe(re.test("\uD800\uDC00\uD800\uDC00abcdx"), false);
    }
}

// 26. Long pattern (large stride) with dense surrogate pairs
//     Stride > 4 means BM skips multiple code units. With dense surrogate pairs,
//     many strides land on surrogates. readCharacterRaw reads raw code units;
//     tryReadUnicodeChar would decode pairs or return errorCodePoint.
{
    let re = /abcdefghij/u; // stride up to 10
    let prefix = "\u{10000}\u{10001}\u{10002}\u{10003}\u{10004}\u{10005}\u{10006}\u{10007}";
    // prefix = 8 non-BMP chars = 16 code units of surrogate pairs
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test(prefix + "abcdefghij"), true);
        shouldBe(re.test(prefix + "abcdefghix"), false);
        shouldBe(re.test(prefix + prefix + "abcdefghij"), true);
        shouldBe(re.test(prefix), false);
    }
}

// 27. Interleaved surrogate pairs and ASCII, various alignments
//     Ensures BM doesn't produce false negatives regardless of where stride lands.
{
    let re = /mnop/u;
    for (let i = 0; i < 100; i++) {
        // Try every alignment of surrogate pairs relative to the pattern
        shouldBe(re.test("\u{10000}mnop"), true);             // 2 code units before
        shouldBe(re.test("a\u{10000}mnop"), true);            // 3 code units before
        shouldBe(re.test("ab\u{10000}mnop"), true);           // 4 code units before
        shouldBe(re.test("abc\u{10000}mnop"), true);          // 5 code units before
        shouldBe(re.test("\u{10000}\u{10001}mnop"), true);    // 4 code units before
        shouldBe(re.test("a\u{10000}\u{10001}mnop"), true);   // 5 code units before
        shouldBe(re.test("ab\u{10000}\u{10001}mnop"), true);  // 6 code units before
        shouldBe(re.test("\u{10000}mnoo"), false);
    }
}

// 28. CharacterClass with non-BMP characters
//     Non-BMP codepoints hash differently from raw surrogates (readCharacterRaw).
//     BM must setAll() for such classes to avoid false negatives.
{
    // Single non-BMP char in class: 0x10041 & 127 = 65, but high surrogate 0xD801 & 127 = 1
    let re = /[\u{10041}]x/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10041}x"), true);
        shouldBe(re.test("xx\u{10041}x"), true);
        shouldBe(re.test("\u{10041}y"), false);
        shouldBe(re.test("ax"), false);
    }
}

// 29. CharacterClass with both BMP and non-BMP characters
{
    let re = /[a\u{10000}]bcd/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("abcd"), true);
        shouldBe(re.test("\u{10000}bcd"), true);
        shouldBe(re.test("xx\u{10000}bcd"), true);
        shouldBe(re.test("xxabcd"), true);
        shouldBe(re.test("xbcd"), false);
        shouldBe(re.test("bbcd"), false);
    }
}

// 30. Non-BMP character class range
{
    let re = /[\u{10000}-\u{10080}]abc/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}abc"), true);
        shouldBe(re.test("\u{10040}abc"), true);
        shouldBe(re.test("\u{10080}abc"), true);
        shouldBe(re.test("\u{10081}abc"), false);
        shouldBe(re.test("xabc"), false);
        shouldBe(re.test("xx\u{10040}abc"), true);
    }
}

// 31. Non-BMP class with v-flag
{
    let re = /[\u{10041}]xyz/v;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10041}xyz"), true);
        shouldBe(re.test("xx\u{10041}xyz"), true);
        shouldBe(re.test("\u{10041}xyy"), false);
    }
}

// 32. Non-BMP class followed by long BMP suffix (larger stride)
{
    let re = /[\u{10000}]abcdefg/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}abcdefg"), true);
        shouldBe(re.test("xxx\u{10000}abcdefg"), true);
        shouldBe(re.test("\u{10001}\u{10000}abcdefg"), true);
        shouldBe(re.test("\u{10000}abcdefx"), false);
        shouldBe(re.test("xabcdefg"), false);
    }
}

// 33. exec() with non-BMP character class — verify index
{
    let re = /[\u{10041}]x/u;
    for (let i = 0; i < 100; i++) {
        let result = re.exec("aa\u{10041}x");
        shouldBe(result !== null, true);
        shouldBe(result[0], "\u{10041}x");
        shouldBe(result.index, 2);

        result = re.exec("\u{10000}\u{10041}x");
        shouldBe(result !== null, true);
        shouldBe(result[0], "\u{10041}x");
        shouldBe(result.index, 2);

        result = re.exec("aax");
        shouldBe(result, null);
    }
}

// 34. Unicode property escape \p{L} — creates a large CharacterClass with both BMP and non-BMP
{
    let re = /\p{L}abcd/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("xabcd"), true);
        shouldBe(re.test("\u{10000}abcd"), true);
        shouldBe(re.test("\u{10300}abcd"), true);  // Old Italic (non-BMP Letter)
        shouldBe(re.test("\u4E00abcd"), true);      // CJK (BMP Letter)
        shouldBe(re.test("1abcd"), false);           // digit, not \p{L}
        shouldBe(re.test("\u{10000}\u{10300}abcd"), true);
    }
}

// 35. Dot (.) in Unicode mode — m_anyCharacter → setAll, BM should still work for other positions
{
    let re = /.abcdef/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("xabcdef"), true);
        shouldBe(re.test("\u{10000}abcdef"), true);
        shouldBe(re.test("\nabcdef"), false);  // . does not match \n without /s flag
        shouldBe(re.test("\u{10000}\u{10001}xabcdef"), true);
        shouldBe(re.test("abcdef"), false);   // need at least 1 char before abcdef
    }
}

// 36. Greedy quantifier ? causing shortenLength
//     /a?bcde/u — 'a?' is greedy with quantityMaxCount=1, so BM info is shortened after position 0
{
    let re = /a?bcde/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("abcde"), true);
        shouldBe(re.test("bcde"), true);
        shouldBe(re.test("\u{10000}abcde"), true);
        shouldBe(re.test("\u{10000}bcde"), true);
        shouldBe(re.test("\u{10000}\u{10001}abcde"), true);
        shouldBe(re.test("xcde"), false);
    }
}

// 37. Global flag + Unicode — multiple matches with surrogates between them
{
    let re = /abc/gu;
    for (let i = 0; i < 100; i++) {
        let str = "abc\u{10000}abc\u{10001}\u{10002}abc";
        let matches = str.match(re);
        shouldBe(matches !== null, true);
        shouldBe(matches.length, 3);
        shouldBe(matches[0], "abc");
        shouldBe(matches[1], "abc");
        shouldBe(matches[2], "abc");
    }
}

// 38. Global + exec with index verification across surrogates
{
    let re = /abc/gu;
    for (let i = 0; i < 100; i++) {
        re.lastIndex = 0;
        let str = "abc\u{10000}abc";
        // Match 1: index 0
        let r1 = re.exec(str);
        shouldBe(r1.index, 0);
        // Match 2: index 5 (\u{10000} = 2 code units, so 3 + 2 = 5)
        let r2 = re.exec(str);
        shouldBe(r2.index, 5);
        // No more matches
        let r3 = re.exec(str);
        shouldBe(r3, null);
    }
}

// 39. Sticky flag + Unicode — BM should NOT be used (sticky checks only at lastIndex)
{
    let re = /abc/yu;
    for (let i = 0; i < 100; i++) {
        re.lastIndex = 0;
        shouldBe(re.test("abc"), true);
        re.lastIndex = 0;
        shouldBe(re.test("xabc"), false);  // sticky: must match at position 0
        re.lastIndex = 2;
        shouldBe(re.test("\u{10000}abc"), true);  // \u{10000} = 2 code units, abc at index 2
    }
}

// 40. Very short pattern (stride=2) with non-BMP input
{
    let re = /xy/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}xy"), true);
        shouldBe(re.test("\u{10000}\u{10001}xy"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}xy"), true);
        shouldBe(re.test("xy"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}"), false);
        shouldBe(re.test("xz"), false);
    }
}

// 41. Multiple consecutive non-BMP CharacterClasses
{
    let re = /[\u{10000}][\u{10001}]abc/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}\u{10001}abc"), true);
        shouldBe(re.test("xx\u{10000}\u{10001}abc"), true);
        shouldBe(re.test("\u{10000}\u{10002}abc"), false);
        shouldBe(re.test("\u{10001}\u{10001}abc"), false);
    }
}

// 42. String.prototype.replace with /u
{
    let re = /abcd/u;
    for (let i = 0; i < 100; i++) {
        shouldBe("\u{10000}\u{10001}abcd".replace(re, "XXXX"), "\u{10000}\u{10001}XXXX");
        shouldBe("\u{10000}\u{10001}efgh".replace(re, "XXXX"), "\u{10000}\u{10001}efgh");
    }
}

// 43. String.prototype.search with /u
{
    let re = /abcd/u;
    for (let i = 0; i < 100; i++) {
        shouldBe("\u{10000}\u{10001}abcd".search(re), 4);
        shouldBe("\u{10000}\u{10001}efgh".search(re), -1);
    }
}

// 44. String.prototype.split with /u
{
    let re = /abcd/u;
    for (let i = 0; i < 100; i++) {
        let parts = "\u{10000}abcd\u{10001}".split(re);
        shouldBe(parts.length, 2);
        shouldBe(parts[0], "\u{10000}");
        shouldBe(parts[1], "\u{10001}");
    }
}

// 45. Non-capturing group + Unicode
{
    let re = /(?:abc)defgh/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}abcdefgh"), true);
        shouldBe(re.test("\u{10000}\u{10001}abcdefgh"), true);
        shouldBe(re.test("abcdefgh"), true);
        shouldBe(re.test("abcdefgx"), false);
    }
}

// 46. ignoreCase with characters that fold to CharacterClass in Unicode mode
//     ß (U+00DF) folds to SS in Unicode mode, creating a CharacterClass
{
    let re = /xyzw/iu;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}XYZW"), true);
        shouldBe(re.test("\u{10000}xyzw"), true);
        shouldBe(re.test("\u{10000}XyZw"), true);
        shouldBe(re.test("\u{10000}xyza"), false);
    }
}

// 47. Mixed: replace with global + unicode + non-BMP input
{
    let re = /abc/gu;
    for (let i = 0; i < 100; i++) {
        let result = "\u{10000}abc\u{10001}abc\u{10002}".replace(re, "X");
        shouldBe(result, "\u{10000}X\u{10001}X\u{10002}");
    }
}

// 48. Pattern with many alternatives (wide merged bitmap)
{
    let re = /abcde|fghij|klmno|pqrst/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}abcde"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}fghij"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}klmno"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}pqrst"), true);
        shouldBe(re.test("\u{10000}\u{10001}\u{10002}uvwxy"), false);
    }
}

// 49. Exhaustive: all prefix lengths 0-20 with non-BMP, various pattern lengths
{
    for (let patLen = 2; patLen <= 8; patLen++) {
        let pat = "abcdefgh".substring(0, patLen);
        let re = new RegExp(pat, "u");
        for (let prefixLen = 0; prefixLen <= 20; prefixLen++) {
            let prefix = "";
            for (let j = 0; j < prefixLen; j++)
                prefix += String.fromCodePoint(0x10000 + j);
            for (let i = 0; i < 10; i++) {
                shouldBe(re.test(prefix + pat), true);
            }
        }
    }
}

// 50. Exhaustive: mixed ASCII and non-BMP prefixes, all alignments
{
    let re = /abcdef/u;
    for (let asciiLen = 0; asciiLen <= 6; asciiLen++) {
        for (let nonBmpLen = 0; nonBmpLen <= 4; nonBmpLen++) {
            let prefix = "x".repeat(asciiLen);
            for (let j = 0; j < nonBmpLen; j++)
                prefix += String.fromCodePoint(0x10000 + j);
            for (let i = 0; i < 10; i++) {
                shouldBe(re.test(prefix + "abcdef"), true);
                shouldBe(re.test(prefix + "abcdex"), false);
            }
        }
    }
}

// 51. Variable-width term (".", non-BMP-capable class) before BMP literals
//     A term that can match a non-BMP code point consumes 1 or 2 code units, so the
//     code unit offsets of subsequent terms are not fixed. The BM window must not
//     extend past such a term, otherwise the prefilter produces false negatives.
{
    let re = /(.A)\1/u;
    for (let i = 0; i < 100; i++) {
        let m = re.exec("\u{10000}A\u{10000}A");
        shouldBe(m !== null, true);
        shouldBe(m[0], "\u{10000}A\u{10000}A");
        shouldBe(m[1], "\u{10000}A");
        shouldBe(re.exec("\u{10000}A\u{10001}A"), null);
    }
}
{
    let re = /.AXY/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}AXY"), true);
        shouldBe(re.exec("\u{10000}AXY")[0], "\u{10000}AXY");
        shouldBe(re.test("ZAXY"), true);
        shouldBe(re.test("\u{10000}AXZ"), false);
    }
}

// 52. Mixed BMP/non-BMP character class before BMP literals
{
    let re = /[あ\u{1F600}]xyz/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{1F600}xyz"), true);
        shouldBe(re.test("あxyz"), true);
        shouldBe(re.test("padding\u{1F600}xyz"), true);
        shouldBe(re.test("あxyw"), false);
    }
}

// 53. Inverted character class (can match non-BMP) before BMP literals
{
    let re = /[^あ]xyz/u;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{1F600}xyz"), true);
        shouldBe(re.test("Qxyz"), true);
        shouldBe(re.test("あxyz"), false);
    }
}

// 54. dotAll (matches any code point) before BMP literals
{
    let re = /.ABC/su;
    for (let i = 0; i < 100; i++) {
        shouldBe(re.test("\u{10000}ABC"), true);
        shouldBe(re.test("\nABC"), true);
        shouldBe(re.test("\u{10000}ABD"), false);
    }
}
