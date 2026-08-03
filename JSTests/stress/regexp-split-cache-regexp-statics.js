// The RegExp split cache skips the match loop on a hit, so it must replay the legacy RegExp
// statics that the loop would have updated. A match can update those statics without producing
// any element (a match at end of input, or a zero-length match at the current position), so the
// replay cannot be inferred from the result length.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

function shouldBeArray(actual, expected) {
    if (!Array.isArray(actual))
        throw new Error(`bad value: expected an array but got ${actual}`);
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function statics() {
    return [RegExp.input, RegExp.lastMatch, RegExp.leftContext, RegExp.rightContext, RegExp.$1, RegExp.$2].join(" ");
}

// Overwrites every static with an unrelated match, so a missing replay is visible.
function clobberStatics() {
    /seed(s)/.test("hello seeds world");
    shouldBe(RegExp.input, "hello seeds world");
}

function split(subject, regexp) {
    clobberStatics();
    const result = subject.split(regexp);
    return { result, statics: statics() };
}

const cases = [
    // Matches at end of input: records statics, pushes nothing.
    { subject: "abc", regexp: /$/ },
    // Zero-length match at position 0: records statics, pushes nothing.
    { subject: "abc", regexp: /(?=abc)/ },
    { subject: "abc", regexp: /^/ },
    // Ordinary separators.
    { subject: "abc", regexp: /b/ },
    { subject: "abc", regexp: /b/y },
    { subject: "a,b,c", regexp: /,/ },
    // Capture groups that participate: the result stays cacheable and $1 must be replayed.
    { subject: "abc", regexp: /(b)/ },
    // A non-participating group yields undefined, which the atom-strings butterfly cannot hold, so
    // the entry is cached with the generic contiguous structure instead.
    { subject: "abc", regexp: /(x)|(b)/ },
    // No match at all: nothing to replay, statics must stay clobbered.
    { subject: "abc", regexp: /zzz/ },
];

function check(testCase) {
    const observed = split(testCase.subject, testCase.regexp);
    shouldBeArray(observed.result, testCase.reference.result);
    if (observed.statics !== testCase.reference.statics)
        throw new Error(`stale RegExp statics for ${testCase.subject}.split(${testCase.regexp}): expected ${JSON.stringify(testCase.reference.statics)} but got ${JSON.stringify(observed.statics)}`);
}

// One case at a time: the cache is keyed by the subject's hash, so interleaving several regexps
// over one subject would evict each entry before it is ever read back.
for (const testCase of cases) {
    // The first split of a given (subject, regexp) pair in this process cannot be a cache hit, so
    // it is the reference for the repeats that follow.
    testCase.reference = split(testCase.subject, testCase.regexp);
    for (let i = 0; i < 1e4; ++i)
        check(testCase);

    // Publishing again after the cache is dropped must behave the same way.
    if (typeof gc === "function") {
        gc();
        for (let i = 0; i < 100; ++i)
            check(testCase);
    }
}

// Pin down the reference behavior itself, so a bug shared by both paths is still caught.
shouldBeArray("abc".split(/$/), ["abc"]);
shouldBe(RegExp.input, "abc");
shouldBe(RegExp.lastMatch, "");
shouldBe(RegExp.leftContext, "abc");
shouldBe(RegExp.rightContext, "");

shouldBeArray("abc".split(/(?=abc)/), ["abc"]);
shouldBe(RegExp.input, "abc");
shouldBe(RegExp.lastMatch, "");
shouldBe(RegExp.leftContext, "");
shouldBe(RegExp.rightContext, "abc");

shouldBeArray("abc".split(/(b)/), ["a", "b", "c"]);
shouldBe(RegExp.lastMatch, "b");
shouldBe(RegExp.$1, "b");

shouldBeArray("abc".split(/(x)|(b)/), ["a", undefined, "b", "c"]);
shouldBe(RegExp.lastMatch, "b");
shouldBe(RegExp.$1, "");
shouldBe(RegExp.$2, "b");

// Splitting never touches the receiver's lastIndex, cached or not.
for (const regexp of [/b/, /b/g, /b/y]) {
    regexp.lastIndex = 2;
    for (let i = 0; i < 1e3; ++i) {
        "abc".split(regexp);
        shouldBe(regexp.lastIndex, 2);
    }
}

// A cache hit hands back a copy-on-write array; writing to it must not corrupt later results.
for (let i = 0; i < 1e3; ++i) {
    const array = "a,b,c".split(/,/);
    shouldBeArray(array, ["a", "b", "c"]);
    array[1] = "mutated";
    array.push("appended");
}
shouldBeArray("a,b,c".split(/,/), ["a", "b", "c"]);
