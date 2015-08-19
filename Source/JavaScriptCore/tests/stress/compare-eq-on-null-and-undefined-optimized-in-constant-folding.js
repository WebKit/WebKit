"use strict"

function unreachableCodeTest() {
    var a;

    var b = null;
    if (b) {
        a = 5;
    }
    return a == b;
}
noInline(unreachableCodeTest);

for (let i = 0; i < 1e4; ++i) {
    if (!unreachableCodeTest())
        throw "Failed unreachableCodeTest() with i = " + i;
}


function inlinedCompareToNull(a) {
    return a == null;
}

function inlinedComparedToUndefined(a) {
    return a == undefined;
}

// Warmup. Litter the profile with every types.
function warmupInlineFunctions() {
    let returnValue = 0;
    for (let i = 0; i < 1e4; ++i) {
        returnValue += inlinedCompareToNull("foo");
        returnValue += inlinedCompareToNull(null);
        returnValue += inlinedCompareToNull(Math);
        returnValue += inlinedCompareToNull(5);
        returnValue += inlinedCompareToNull(5.5);

        returnValue += inlinedComparedToUndefined("foo");
        returnValue += inlinedComparedToUndefined(null);
        returnValue += inlinedComparedToUndefined(Math);
        returnValue += inlinedComparedToUndefined(5);
        returnValue += inlinedComparedToUndefined(5.5);
    }
    return returnValue;
}
noInline(warmupInlineFunctions);
warmupInlineFunctions();

function testInlineFunctions(undefinedArg, nullArg) {
    if (inlinedCompareToNull("foo"))
        throw "Failed inlinedCompareToNull(\"foo\")";

    if (!inlinedCompareToNull(null))
        throw "Failed !inlinedCompareToNull(null)";

    if (!inlinedCompareToNull(undefined))
        throw "Failed !inlinedCompareToNull(undefined)";

    if (!inlinedCompareToNull(undefinedArg))
        throw "Failed !inlinedCompareToNull(undefinedArg)";

    if (!inlinedCompareToNull(nullArg))
        throw "Failed !inlinedCompareToNull(nullArg)";

}
noInline(testInlineFunctions);

for (let i = 0; i < 1e4; ++i) {
    testInlineFunctions(undefined, null);
}