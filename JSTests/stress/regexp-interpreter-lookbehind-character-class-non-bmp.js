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

function shouldMatch(re, string, expected) {
    shouldBe(stringify(re.exec(string)), expected);
}

shouldMatch(/(?<=[\u{1F600}a])b/u, "x\ud83d\ude00b", "[3,\"b\"]");
shouldMatch(/(?<=[\u{1F600}a])b/u, "xab", "[2,\"b\"]");
shouldMatch(/(?<=[\u{1F600}a])b/v, "x\ud83d\ude00b", "[3,\"b\"]");
shouldMatch(/(?<=[\u{1F600}a])b/u, "x\ud83d\ude01b", "null");
shouldMatch(/(?<=\p{L})b/u, "\ud801\udc28b", "[2,\"b\"]");
shouldMatch(/(?<!\p{L})b/u, "\ud801\udc28b", "null");
shouldMatch(/(?<!\p{L})b/u, "1b", "[1,\"b\"]");
shouldMatch(/(?<=[\u{10400}x])b/u, "\ud801\udc00b", "[2,\"b\"]");
shouldMatch(/(?<=[\u{1F600}-\u{1F64F}]{2})x/u, "\ud83d\ude00\ud83d\ude4fx", "[4,\"x\"]");
shouldMatch(/(?<=[\u{1F600}-\u{1F64F}]{2})x/u, "\ud83d\ude00x", "null");
shouldMatch(/(?<=^[\s\S])$/u, "\ud83d\ude00", "[2,\"\"]");
shouldMatch(/(?<=[\s\S]{2})x/u, "a\ud83d\ude00x", "[3,\"x\"]");
shouldMatch(/(?<=[\s\S]{2})x/u, "\ud83d\ude00x", "null");
shouldMatch(/(?<=^[\s\S]{3})x/u, "a\ud83d\ude00x", "null");
shouldMatch(/(?<=[^a]{2})x/u, "\ud83d\ude00\ud83d\ude01x", "[4,\"x\"]");
shouldMatch(/(?<=[^a]{2})x/u, "b\ud83d\ude01x", "[3,\"x\"]");
shouldMatch(/(?<=.)a/su, "\ud801\udc00a", "[2,\"a\"]");
shouldMatch(/(?<=\u{1F600}.)x/su, "\ud83d\ude00\ud83d\ude01x", "[4,\"x\"]");
shouldMatch(/(?<=\p{L}\u{1F600}.)x/su, "\u00e9\ud83d\ude00\ud83d\ude01x", "[5,\"x\"]");
