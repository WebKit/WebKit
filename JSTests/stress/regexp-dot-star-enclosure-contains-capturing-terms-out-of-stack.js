// This tests that containsCapturingTerms() used by the DotStarEnclosure optimization
// throws when we run out of stack space instead of crashing.
//@ exclusive!
//@ requireOptions("-e", "let depth=25000") if $memoryLimited

depth = typeof(depth) === 'undefined' ? 200000 : depth;

let expectedException = "SyntaxError: Invalid regular expression: regular expression too large";

function test(source)
{
    try {
        new RegExp(source);
    } catch (e) {
        if (e != expectedException)
            throw "Expected \"" + expectedException + "\", but got \"" + e + "\" for: " + source.slice(0, 30) + "...";
    }
}

test(".*" + "(?:".repeat(depth) + "a" + ")".repeat(depth) + ".*");
test("^.*" + "(?:".repeat(depth) + "a" + ")".repeat(depth) + ".*$");
