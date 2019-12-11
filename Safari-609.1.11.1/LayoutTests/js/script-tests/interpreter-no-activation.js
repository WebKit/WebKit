description("Make sure arguments tearoff works correctly when a functions activation has not been created.");

function f1(a,b,c,d) {
    if (0) (function (){ a; });
    a = true;
    return arguments[0];
}
shouldBeTrue("f1(false)");

function f2(a,b,c,d) {
    "use strict";
    a = false;
    if (0) (function (){ a; });
    return arguments[0];
}
shouldBeTrue("f2(true)");
testPassed("Didn't crash");
