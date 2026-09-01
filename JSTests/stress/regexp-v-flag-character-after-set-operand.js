function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function shouldThrowSyntaxError(source) {
    try {
        new RegExp(source, "v");
    } catch (error) {
        if (error instanceof SyntaxError)
            return;
        throw new Error(`bad error for /${source}/v: ${error}`);
    }
    throw new Error(`/${source}/v did not throw`);
}

// A character following a class set operand (a nested class or a \q{} string
// disjunction) must add only itself, not a stale cached character.
for (const [source, matching, nonMatching] of [
    ["^[\\q{ab}c]$", ["ab", "c"], ["\0", "a", "b", "abc"]],
    ["^[\\q{ab|cd}e]$", ["ab", "cd", "e"], ["\0", "abe"]],
    ["^[[ab]c]$", ["a", "b", "c"], ["\0", "d"]],
    ["^[[\\p{L}]1]$", ["a", "é", "1"], ["\0", "2"]],
    ["^[[a-c][x]y]$", ["a", "b", "c", "x", "y"], ["\0", "d"]],
    ["^[a[b]c]$", ["a", "b", "c"], ["\0", "d"]],
    ["^[\\d\\q{xy}z]$", ["0", "9", "xy", "z"], ["\0", "x"]],
    ["^[[^a]b]$", ["b", "c", "\0"], ["a"]],
    ["^[^[a]b]$", ["c", "\0"], ["a", "b"]],
]) {
    const regExp = new RegExp(source, "v");
    for (const string of matching)
        shouldBe(regExp.test(string), true);
    for (const string of nonMatching)
        shouldBe(regExp.test(string), false);
}

shouldThrowSyntaxError("[\\q{ab}-c]");
shouldThrowSyntaxError("[[a]-c]");
shouldThrowSyntaxError("[a&&[b]c]");
shouldThrowSyntaxError("[a--[b]c]");
