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

shouldMatch(/(?<=$.*)/su, "\ud83d\ude01", "[2,\"\"]");
shouldMatch(/(?<=$.*)/su, "a\ud83d\ude01", "[3,\"\"]");
shouldMatch(/(?<=^.*)b/su, "\ud83d\ude00ab", "[3,\"b\"]");
shouldMatch(/(?<=[^x]+)x/u, "a\ud83d\ude00bx", "[4,\"x\"]");
shouldMatch(/(?<=^[^x]+?)x/u, "a\ud83d\ude00bx", "[4,\"x\"]");
shouldMatch(/(?<=(.*))x/su, "\ud83d\ude00\ud83d\ude01x", "[4,\"x\",\"\ud83d\ude00\ud83d\ude01\"]");
shouldMatch(/(?<=(.*?))x/su, "\ud83d\ude00\ud83d\ude01x", "[4,\"x\",\"\"]");
shouldMatch(/(?<=\u{1F600}*)$/u, "\ud83d\ude00", "[2,\"\"]");
shouldMatch(/(?<=[\S]*)$/u, "\ud83d\ude00", "[2,\"\"]");
shouldMatch(/(?<=[\S]+)$/u, "\ud83d\ude00", "[2,\"\"]");
shouldBe("\ud83d\ude4f\ud83d\ude4f".replace(/(?<=.)/gsu, "|"), "\ud83d\ude4f|\ud83d\ude4f|");
shouldBe("a\ud83d\ude00b".replace(/(?<=[^]*)/gu, "|"), "|a|\ud83d\ude00|b|");
shouldBe(JSON.stringify([..."\ud83d\ude00".matchAll(/(?<=[\S])/gu)].map((match) => match.index)), "[2]");
shouldBe(JSON.stringify([..."\ud83d\ude00\ud83d\ude01".matchAll(/(?<=.)/gsu)].map((match) => match.index)), "[2,4]");
