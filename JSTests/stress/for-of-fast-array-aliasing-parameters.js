// While an Array is destructured or looped over without an iterator object, the Array is read back from a register at
// every step. Nothing the program does to the binding the Array came from (parameters through `arguments`, eval, with,
// closures, assignments in default values and in the loop body) may change which Array is being iterated.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const other = ["x", "y", "z"];
// Sloppy function, simple parameter list: `arguments` is mapped to the parameters.
function sloppySimple(p) {
    var [a, b = (arguments[0] = other, p = other, "d"), c] = p;
    return [a, b, c, p === other].join();
}
function sloppySimpleLoop(p) {
    var seen = [];
    for (var x of p) { seen.push(x); arguments[0] = other; p = other; }
    return seen.join() + "|" + (p === other);
}
// Non-simple parameter list: `arguments` is not mapped.
function sloppyPattern([a, b = (arguments[0] = other, "d"), c]) {
    return [a, b, c, arguments[0] === other].join();
}
function sloppyPatternDefault([a, b = (arguments[0] = other, "d"), c] = other) {
    return [a, b, c].join();
}
function strictSimple(p) {
    "use strict";
    var [a, b = (arguments[0] = other, p = other, "d"), c] = p;
    return [a, b, c, p === other].join();
}
function arrowWithRest(...args) {
    var f = ([a, b = (args[0] = other, args = other, "d"), c]) => [a, b, c].join();
    return f(args[0]);
}

for (var i = 0; i < testLoopCount; i++) {
    shouldBe(sloppySimple([1, , 3]), "1,d,3,true");
    shouldBe(sloppySimple([1, 2, 3]), "1,2,3,false");
    shouldBe(sloppySimpleLoop([1, 2, 3]), "1,2,3|true");
    shouldBe(sloppyPattern([1, , 3]), "1,d,3,true");
    shouldBe(sloppyPatternDefault([1, , 3]), "1,d,3");
    shouldBe(sloppyPatternDefault(), "x,y,z");
    shouldBe(strictSimple([1, , 3]), "1,d,3,true");
    shouldBe(arrowWithRest([1, , 3]), "1,d,3");
}
