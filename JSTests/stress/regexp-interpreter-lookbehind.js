//@ runDefault("--useRegExpJIT=0")
//@ runDefault

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + ", expected " + expected);
}

function stringify(match) {
    if (match === null)
        return "null";
    return "[" + [match.index, ...Array.from(match, (x) => x === undefined ? "undefined" : JSON.stringify(x))].join(",") + "]";
}

function shouldMatch(re, string, lastIndex, expected) {
    re.lastIndex = lastIndex;
    shouldBe(stringify(re.exec(string)), expected);
}

shouldMatch(/(?<=(a)(b))c/, "xabc", 0, "[3,\"c\",\"a\",\"b\"]");
shouldMatch(/(?<=(a|ab)+)c/, "xababc", 0, "[5,\"c\",\"ab\"]");
shouldMatch(/(?<=(?:(a)|(b))+)c/, "abc", 0, "[2,\"c\",\"a\",undefined]");
shouldMatch(/(?<=((a)|b)*)c/, "abbc", 0, "[3,\"c\",\"a\",\"a\"]");
shouldMatch(/(?<=(\w)(\w))\d/, "ab1", 0, "[2,\"1\",\"a\",\"b\"]");
shouldMatch(/(?<=(a)x(\1))y/, "axay", 0, "null");
shouldMatch(/(a)x(?<=\1x)/u, "axax", 0, "[0,\"ax\",\"a\"]");
shouldMatch(/(?<=(\u{1F600})\1)x/u, "\ud83d\ude00\ud83d\ude00x", 0, "[4,\"x\",\"\ud83d\ude00\"]");
shouldMatch(/(?<=\1(a))x/, "aax", 0, "[2,\"x\",\"a\"]");
shouldMatch(/(?<=(?<n>\d+)\.)\d+/, "3.14", 0, "[2,\"14\",\"3\"]");
shouldMatch(/(?<=(a+)b)c/, "aaabc", 0, "[4,\"c\",\"aaa\"]");
shouldMatch(/(?<=(a+?)b)c/, "aaabc", 0, "[4,\"c\",\"a\"]");
shouldMatch(/(?<=(ab){2})c/, "ababc", 0, "[4,\"c\",\"ab\"]");
shouldMatch(/(?<=(ab){1,2}?)c/, "ababc", 0, "[4,\"c\",\"ab\"]");
shouldMatch(/(?<=(?:ab)+)c/, "xababc", 0, "[5,\"c\"]");
shouldMatch(/(?<=(?<=x)a)b/, "xab", 0, "[2,\"b\"]");
shouldMatch(/(?<=(?<=x)a)b/, "yab", 0, "null");
shouldMatch(/(?<=(?<!x)a)b/, "yab", 0, "[2,\"b\"]");
shouldMatch(/(?<=a(?=b)b)c/, "abc", 0, "[2,\"c\"]");
shouldMatch(/(?<=a(?!c)b)c/, "abc", 0, "[2,\"c\"]");
shouldMatch(/(?<=\b(?!un)\w+)able/, "unable readable", 0, "[11,\"able\"]");
shouldMatch(/(?<=(?=\u{1F600})\u{1F600})b/u, "\ud83d\ude00b", 0, "[2,\"b\"]");
shouldMatch(/(?<=a(?=[\s\S]{2})\u{1F600})b/u, "a\ud83d\ude00b", 0, "[3,\"b\"]");
shouldMatch(/(?<=(?<=\u{1F600})a)b/u, "\ud83d\ude00ab", 0, "[3,\"b\"]");
shouldMatch(/(?<=^ab)c/, "abc", 0, "[2,\"c\"]");
shouldMatch(/(?<=^ab)c/m, "x\u000aabc", 0, "[4,\"c\"]");
shouldMatch(/(?<=ab$)/m, "ab\u000ac", 0, "[2,\"\"]");
shouldMatch(/(?<=\bab)c/, "x abc", 0, "[4,\"c\"]");
shouldMatch(/(?<=\Bab)c/, "xabc", 0, "[3,\"c\"]");
shouldMatch(/(?<=^)x/, "x", 0, "[0,\"x\"]");
shouldMatch(/(?<=$)/, "ab", 0, "[2,\"\"]");
shouldMatch(/ab(?<=ab)c/, "abc", 0, "[0,\"abc\"]");
shouldMatch(/a(?<=xa)b/, "xab", 0, "[1,\"ab\"]");
shouldMatch(/a(?<!xa)b/, "xab", 0, "null");
shouldMatch(/x(?<!ab)cd/, "xcd", 0, "[0,\"xcd\"]");
shouldMatch(/ab(?<!b)c|abc/, "abc", 0, "[0,\"abc\"]");
shouldMatch(/(?<!a)b/, "ab cb", 0, "[4,\"b\"]");
shouldMatch(/(?<!\u{1F600})b/u, "\ud83d\ude00b ab", 0, "[5,\"b\"]");
shouldMatch(/(?<=\u{1F600})b/gu, "\ud83d\ude00b\ud83d\ude00b", 0, "[2,\"b\"]");
shouldMatch(/(?<=\u{1F600})b/gu, "\ud83d\ude00b\ud83d\ude00b", 1, "[2,\"b\"]");
shouldMatch(/(?<=\u{1F600})b/gu, "\ud83d\ude00b\ud83d\ude00b", 2, "[2,\"b\"]");
shouldMatch(/(?<=\u{1F600})b/gu, "\ud83d\ude00b\ud83d\ude00b", 3, "[5,\"b\"]");
shouldMatch(/(?<=a|bc)x/, "bcx", 0, "[2,\"x\"]");
shouldMatch(/(?<=a|bc|)x/, "x", 0, "[0,\"x\"]");
shouldMatch(/(?<=(?:a|bcd)e)f/, "bcdef", 0, "[4,\"f\"]");
