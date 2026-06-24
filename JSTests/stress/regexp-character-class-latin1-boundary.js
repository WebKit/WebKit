function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function repeat(fn) {
    for (let i = 0; i < testLoopCount; ++i)
        fn();
}

repeat(() => {
    shouldBe(/abc/i.test("ABC"), true, "ASCII /i positive");
    shouldBe(/abc/i.test("abd"), false, "ASCII /i negative");
});

repeat(() => {
    shouldBe(/à/i.test("À"), true, "à/i matches À");
    shouldBe(/À/i.test("à"), true, "À/i matches à");
    shouldBe(/[À-Þ]/i.test("é"), true, "[À-Þ]/i matches é");
    shouldBe(/[À-Þ]/.test("é"), false, "[À-Þ] does not match é");
});

repeat(() => {
    shouldBe(/[þÿ]/.test("þ"), true, "0xFE in [0xFE,0xFF]");
    shouldBe(/[þÿ]/.test("ÿ"), true, "0xFF in [0xFE,0xFF]");
    shouldBe(/[þÿ]/.test("Ā"), false, "0x100 not in [0xFE,0xFF]");
    shouldBe(/Ā/.test("Ā"), true, "literal 0x100");
    shouldBe(/[Ā-ſ]/.test("ŝ"), true, "non-Latin-1 range matches");
    shouldBe(/[Ā-ſ]/.test("ÿ"), false, "non-Latin-1 range excludes 0xFF");
    shouldBe(/[ð-Đ]/.test("ÿ"), true, "boundary-crossing range, Latin-1 side");
    shouldBe(/[ð-Đ]/.test("Ā"), true, "boundary-crossing range, non-Latin-1 side");
    shouldBe(/[ð-Đ]/.test(""), false, "boundary-crossing range below");
    shouldBe(/[ð-Đ]/.test("đ"), false, "boundary-crossing range above");
});

repeat(() => {
    shouldBe(/ÿ/i.test("ÿ"), true, "ÿ/i matches ÿ");
    shouldBe(/ÿ/i.test("Ÿ"), true, "ÿ/i matches Ÿ");
    shouldBe(/Ÿ/i.test("ÿ"), true, "Ÿ/i matches ÿ");
    shouldBe(/Ÿ/iu.test("ÿ"), true, "Ÿ/iu matches ÿ");
});

repeat(() => {
    shouldBe(/µ/i.test("µ"), true, "µ/i matches µ");
    shouldBe(/µ/i.test("Μ"), true, "µ/i matches Μ");
    shouldBe(/µ/i.test("μ"), true, "µ/i matches μ");
    shouldBe(/Μ/i.test("µ"), true, "Μ/i matches µ");
});

repeat(() => {
    shouldBe(/ß/i.test("ß"), true, "ß/i matches ß");
    shouldBe(/ß/i.test("S"), false, "ß/i does not match S in UCS2");
    shouldBe(/ß/i.test("s"), false, "ß/i does not match s in UCS2");
    shouldBe(/ß/iu.test("ẞ"), true, "ß/iu matches U+1E9E");
});

repeat(() => {
    shouldBe(/abcd|wxyz/i.test("AbCd"), true, "masked /i alt 1");
    shouldBe(/abcd|wxyz/i.test("WxYz"), true, "masked /i alt 2");
    shouldBe(/abcd|wxyz/i.test("abce"), false, "masked /i no match");
    shouldBe(/abcd|wxyz/i.test("ééAbCdé"), true, "masked /i with Latin-1 surrounding");
});

repeat(() => {
    shouldBe(/./.test("a"), true, ". matches a");
    shouldBe(/./.test("é"), true, ". matches é");
    shouldBe(/./.test("ÿ"), true, ". matches ÿ");
    shouldBe(/./.test("Ā"), true, ". matches 0x100");
    shouldBe(/./.test("\n"), false, ". does not match newline");
});

repeat(() => {
    shouldBe(/[^a]/.test("é"), true, "[^a] matches é");
    shouldBe(/[^abc]/.test("a"), false, "[^abc] does not match a");
    shouldBe(/[^a-z]/.test("é"), true, "[^a-z] matches é");
    shouldBe(/[^Ā-ſ]/.test("é"), true, "inverted non-Latin-1 range");
    shouldBe(/[^à-ÿ]/.test("ÿ"), false, "inverted Latin-1 range rejects ÿ");
    shouldBe(/[^à-ÿ]/.test("Ā"), true, "inverted Latin-1 range accepts 0x100");
});

repeat(() => {
    shouldBe(/\w/.test("é"), false, "\\w legacy");
    shouldBe(/\W/.test("é"), true, "\\W legacy");
    shouldBe(/\s/.test(" "), true, "\\s NBSP");
    shouldBe(/\d/.test("5"), true, "\\d");
    shouldBe(/\D/.test("é"), true, "\\D");
});

repeat(() => {
    shouldBe(/\p{L}/u.test("é"), true, "/u \\p{L} é");
    shouldBe(/\p{L}/u.test("a"), true, "/u \\p{L} a");
    shouldBe(/\p{L}/u.test("5"), false, "/u \\p{L} digit");
    shouldBe(/\p{L}/u.test("Ā"), true, "/u \\p{L} 0x100");
});

repeat(() => {
    shouldBe(/[\p{L}--\p{ASCII}]/v.test("é"), true, "/v [L--ASCII] é");
    shouldBe(/[\p{L}--\p{ASCII}]/v.test("a"), false, "/v [L--ASCII] a");
    shouldBe(/[\p{L}--\p{ASCII}]/v.test("Ā"), true, "/v [L--ASCII] non-Latin-1");
    shouldBe(/[\p{L}&&\p{ASCII}]/v.test("a"), true, "/v [L&&ASCII] a");
    shouldBe(/[\p{L}&&\p{ASCII}]/v.test("é"), false, "/v [L&&ASCII] é");
});

repeat(() => {
    shouldBe(/[éŸ]/.test("é"), true, "mixed class Char16 Latin-1 hit");
    shouldBe(/[éŸ]/.test("Ÿ"), true, "mixed class Char16 non-Latin-1 hit");
    shouldBe(/[éŸ]/.test("e"), false, "mixed class miss");
});

repeat(() => {
    shouldBe(/[éŸ]/.test("abcé"), true, "Char8 input hits Latin-1 entry");
    shouldBe(/[éŸ]/.test("abcdef"), false, "Char8 input misses");
});

repeat(() => {
    let m = /[À-Þ]+/i.exec("aAbBÀàÁxy");
    shouldBe(m !== null, true, "Latin-1 alpha range exec exists");
    shouldBe(m[0], "ÀàÁ", "Latin-1 alpha range match");
    shouldBe(m.index, 4, "Latin-1 alpha range index");
});

repeat(() => {
    shouldBe(/[^\p{ASCII}]/u.test("a"), false, "/u [^ASCII] a");
    shouldBe(/[^\p{ASCII}]/u.test("é"), true, "/u [^ASCII] é");
    shouldBe(/[^\p{ASCII}]/u.test("Ā"), true, "/u [^ASCII] 0x100");
});
