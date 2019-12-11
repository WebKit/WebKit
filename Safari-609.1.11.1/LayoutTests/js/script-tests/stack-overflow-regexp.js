// https://bugs.webkit.org/show_bug.cgi?id=190755
//@ skip if $architecture == "arm" and $hostOS == "linux"
description('Test that we do not overflow the stack while handling regular expressions');

// Base case.
shouldThrow('new RegExp(Array(50000).join("(") + "a" + Array(50000).join(")"))', '"Error: Out of memory: Invalid regular expression: too many nested disjunctions"');

{ // Verify that a deep JS stack does not help avoiding the error.
    function recursiveCall(depth) {
        if (!(depth % 10)) {
            debug("Creating RegExp at depth " + depth);
            shouldThrow('new RegExp(Array(50000).join("(") + "a" + Array(50000).join(")"))', '"Error: Out of memory: Invalid regular expression: too many nested disjunctions"');
        }
        if (depth < 100) {
            recursiveCall(depth + 1);
        }
    }
    recursiveCall(0);
}

{ // Have the deepest nested subpattern surrounded by other expressions.
    var expression = "";
    for (let i = 0; i < 50000; ++i) {
        expression += "((a)(";
    }
    expression += "b";
    for (let i = 0; i < 50000; ++i) {
        expression += ")(c))";
    }
    shouldThrow('new RegExp(expression)', '"Error: Out of memory: Invalid regular expression: too many nested disjunctions"');
}
