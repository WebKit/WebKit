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

shouldMatch(/(?<=\u{1F600}{2})x/u, "\ud83d\ude00\ud83d\ude00x", "[4,\"x\"]");
shouldMatch(/(?<=\u{1F600}{2})x/v, "\ud83d\ude00\ud83d\ude00x", "[4,\"x\"]");
shouldMatch(/(?<=\u{1F600}{3})x/u, "\ud83d\ude00\ud83d\ude00\ud83d\ude00x", "[6,\"x\"]");
shouldMatch(/(?<=\u{1F600}{2})x/u, "a\ud83d\ude00x", "null");
shouldMatch(/(?<=\u{1F600}{2})x/u, "\ud83d\ude00x", "null");
shouldMatch(/(?<=\u{1F600}{2})x/u, "\ud83d\ude00\ud83d\ude01x", "null");
shouldMatch(/(?<=\u{1F600}{2})x/u, "\ud83d\ude01\ud83d\ude00x", "null");
shouldMatch(/(?<=a\u{1F600}{2}b)x/u, "a\ud83d\ude00\ud83d\ude00bx", "[6,\"x\"]");
shouldMatch(/(?<=\u{1F600}\u{1F601})x/u, "\ud83d\ude00\ud83d\ude01x", "[4,\"x\"]");
shouldMatch(/(?<=\u{1F600}\u{1F601})x/u, "\ud83d\ude01\ud83d\ude00x", "null");
shouldMatch(/(?<=(\u{1F600}{2}))x/u, "\ud83d\ude00\ud83d\ude00x", "[4,\"x\",\"\ud83d\ude00\ud83d\ude00\"]");
shouldMatch(/(?<!\u{1F600}{2})x/u, "\ud83d\ude00\ud83d\ude00x", "null");
shouldMatch(/(?<!\u{1F600}{2})x/u, "a\ud83d\ude00x", "[3,\"x\"]");
