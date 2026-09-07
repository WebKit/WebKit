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

// An escaped hyphen following a class set operand (a nested class or a \q{}
// string disjunction) is a valid ClassSetReservedPunctuator escape.
for (const [source, matching, nonMatching] of [
    ["^[[a]\\-]$", ["a", "-"], ["b", "\0"]],
    ["^[\\q{ab}\\-]$", ["ab", "-"], ["a", "b", "\0"]],
    ["^[[a][b]\\-]$", ["a", "b", "-"], ["c"]],
    ["^[[a]\\-x]$", ["a", "-", "x"], ["b"]],
    ["^[[a]\\-\\-]$", ["a", "-"], ["b"]],
    ["^[\\-[a]]$", ["a", "-"], ["b"]],
    ["^[[\\d]\\-]$", ["0", "-"], ["a"]],
] ) {
    const regExp = new RegExp(source, "v");
    for (const string of matching)
        shouldBe(regExp.test(string), true);
    for (const string of nonMatching)
        shouldBe(regExp.test(string), false);
}

// An unescaped hyphen there is still a syntax error.
shouldThrowSyntaxError("[[a]-]");
shouldThrowSyntaxError("[\\q{ab}-]");
shouldThrowSyntaxError("[[a]-c]");
shouldThrowSyntaxError("[[\\d]-]");
shouldThrowSyntaxError("[[\\d]-c]");
shouldThrowSyntaxError("[\\q{ab}-c]");
