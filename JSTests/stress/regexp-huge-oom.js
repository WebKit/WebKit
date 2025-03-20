//@ skip if $memoryLimited
// Test that throw an OOM exception when compiling / executing a pathologically large RegExp's

function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error("Bad value: , expected \"" + expected + "\", but got \"" + actual + "\"");
}

function shouldThrow(run, errorType)
{
    let actual;
    var hadError = false;

    try {
        actual = run();
    } catch (e) {
        hadError = true;
        actual = e;
    }

    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expected " + run + "() to throw " + errorType.name + " , but threw '" + actual + "'");
}

// This should throw during pattern compilation.
shouldThrow(() => RegExp('a?'.repeat(2**19) + 'b').exec('x'), SyntaxError);

var r1 = RegExp('a?'.repeat(2**19));
shouldBe(JSON.stringify(r1.exec('x')), `[""]`);
shouldBe(JSON.stringify(r1.exec('x')), `[""]`);

