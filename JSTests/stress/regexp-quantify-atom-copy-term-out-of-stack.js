//@ exclusive!
//@ requireOptions("-e", "let depth=25000") if $memoryLimited

depth = typeof(depth) === 'undefined' ? 50000 : depth;

// Either error message is acceptable: deeply nested parens with a non-zero-min
// quantifier exceed pattern size limits at compile time. The exact message
// depends on which limit is hit first ("regular expression too large" via
// pattern offset overflow vs "too many nested disjunctions" via parser depth).
let expectedExceptions = [
    "SyntaxError: Invalid regular expression: regular expression too large",
    "RangeError: Out of memory: Invalid regular expression: too many nested disjunctions",
];

function test(source)
{
    try {
        new RegExp(source);
    } catch (e) {
        if (!expectedExceptions.includes(String(e)))
            throw "Expected one of [" + expectedExceptions.join(", ") + "], but got \"" + e + "\" for: " + source.slice(0, 30) + "...";
    }
}

test("(?:".repeat(depth) + "a" + ")".repeat(depth) + "{1,2}");

test("(?<=" + "(?:".repeat(depth) + "a" + ")".repeat(depth) + "{1,2}" + ")");
