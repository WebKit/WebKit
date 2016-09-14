// Regression test for 161995.

function testStatic()
{
    return /a/Z;
}

try {
    testStatic();
    throw "Expected a SyntaxEerror for bad RegExp flags, but didn't get one.";
} catch(e) {
    if (e != "SyntaxError: Invalid regular expression: invalid flags")
        throw "Incorrect exception for bad RegExp flags.  Got: " + e;
}
