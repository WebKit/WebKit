function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected: " + String(expected) + " actual: " + String(actual));
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

// Non-greedy backreference with finite max count.
// The JIT had a bug where the operands in the max count check were reversed,
// causing the backreference to never match on backtrack.

// \1?? with max=1: should try zero first, then one on backtrack
{
    let result = /^(a)\1??$/.exec("aa");
    shouldBeArray(result, ["aa", "a"]);
}

// \1?? in context: non-greedy should initially skip, then backtrack to match
{
    let result = /(a)\1??b/.exec("aab");
    shouldBe(result.index, 0);
    shouldBeArray(result, ["aab", "a"]);
}

// \1{0,2}? non-greedy with max=2
{
    let result = /^(a)\1{0,2}?$/.exec("aaa");
    shouldBeArray(result, ["aaa", "a"]);
}

// \1{0,3}? non-greedy with max=3, matching exactly 3 repetitions
{
    let result = /^(a)\1{0,3}?$/.exec("aaaa");
    shouldBeArray(result, ["aaaa", "a"]);
}

// Ensure non-greedy prefers fewer matches when possible
{
    let result = /(a)\1{0,2}?b/.exec("aaab");
    shouldBe(result.index, 0);
    shouldBeArray(result, ["aaab", "a"]);
}

// Run in a loop to exercise JIT compilation
for (let i = 0; i < 200; ++i) {
    let result = /^(a)\1??$/.exec("aa");
    shouldBeArray(result, ["aa", "a"]);

    result = /(a)\1??b/.exec("aab");
    shouldBe(result.index, 0);
    shouldBeArray(result, ["aab", "a"]);

    result = /^(a)\1{0,2}?$/.exec("aaa");
    shouldBeArray(result, ["aaa", "a"]);
}
