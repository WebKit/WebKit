//@ exclusive!
//@ requireOptions("-e", "let depth=25000") if $memoryLimited

depth = typeof(depth) === 'undefined' ? 50000 : depth;

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

test("(?:".repeat(depth) + "a" + ")".repeat(depth) + "{1,2}");

test("(?<=" + "(?:".repeat(depth) + "a" + ")".repeat(depth) + "{1,2}" + ")");
