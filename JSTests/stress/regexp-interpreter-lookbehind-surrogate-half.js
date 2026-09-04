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

shouldMatch(/(?<=\uD83D)/u, "\ud83d\ude00", "null");
shouldMatch(/(?<=\uD83D)x/u, "\ud83dx", "[1,\"x\"]");
shouldMatch(/(?<=\uDE00)x/u, "\ud83d\ude00x", "null");
shouldMatch(/(?<=\uDE00)x/u, "\ude00x", "[1,\"x\"]");
shouldMatch(/(?<=[\uDE00])x/u, "\ud83d\ude00x", "null");
shouldMatch(/(?<=[\uD83D])x/u, "\ud83dx", "[1,\"x\"]");
shouldMatch(/(?<=^[\s\S])x/u, "\ude00x", "[1,\"x\"]");
shouldMatch(/(?<=^[\s\S])x/u, "\ud83d\ude00x", "[2,\"x\"]");
shouldMatch(/(?<!\uD83D)/u, "\ud83d\ude00", "[0,\"\"]");
shouldMatch(/(?<=\uD83D)x/, "\ud83d\ude00x", "null");
shouldMatch(/(?<=\uD83D)/, "\ud83d\ude00", "[1,\"\"]");
