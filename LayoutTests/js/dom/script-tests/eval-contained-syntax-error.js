description(
"This file tests whether a syntax error inside an eval() has the correct line number. That line number should not be the offset of an error within an eval, but rather the line of an eval itself. "
);

try {
    eval("a[0]]"); //line 6: error should come from here
} catch (e) {
    if (e.line == 6)
        debug("PASS: e.line should be 6 and is.");
    else
        debug("FAIL: e.line should be 6 but instead is " + e.line + ".");
}
