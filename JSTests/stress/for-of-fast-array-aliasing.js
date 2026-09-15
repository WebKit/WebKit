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
function viaEval(p) {
    var [a, b = (eval("p = other"), "d"), c] = p;
    return [a, b, c, p === other].join();
}
function viaEvalLoop(p) {
    var seen = [];
    for (var x of p) { seen.push(x); eval("p = other; var x2 = 1"); }
    return seen.join();
}
function viaWith(scope) {
    with (scope) {
        var [a, b = (p = other, "d"), c] = p;
        var seen = [];
        for (var x of q) { seen.push(x); q = other; scope.q = other; }
    }
    return [a, b, c].join() + "|" + seen.join() + "|" + (scope.p === other) + (scope.q === other);
}
function viaClosure(p) {
    function set() { p = other; }
    var [a, b = (set(), "d"), c] = p;
    var seen = [];
    for (var x of p = [7, 8, 9]) { seen.push(x); set(); }
    return [a, b, c].join() + "|" + seen.join();
}
function selfAssign(p) {
    [p, p] = p;
    return String(p);
}
function selfAssignLoop(p) {
    var seen = [];
    for (p of p) seen.push(p);
    return seen.join() + "|" + p;
}
function nestedSame(p) {
    var [[a, b], [c, d] = p, e = (p = other)] = p;
    return [a, b, c, d, e === other].join();
}
function catchParameter(p) {
    try { throw p; } catch ([a, b = (p = other, "d"), c]) { return [a, b, c].join(); }
}
var globalArray;
function viaGlobal() {
    var seen = [];
    for (var x of globalArray) { seen.push(x); globalArray = other; }
    var [a, b = (globalArray = [0], "d")] = globalArray;
    return seen.join() + "|" + a + b;
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
    shouldBe(viaEval([1, , 3]), "1,d,3,true");
    shouldBe(viaEvalLoop([1, 2, 3]), "1,2,3");
    shouldBe(viaWith({ p: [1, , 3], q: [4, 5, 6] }), "1,d,3|4,5,6|truetrue");
    shouldBe(viaClosure([1, , 3]), "1,d,3|7,8,9");
    shouldBe(selfAssign([1, 2, 3]), "2");
    shouldBe(selfAssignLoop([1, 2, 3]), "1,2,3|3");
    shouldBe(nestedSame([[1, 2], , ,]), "1,2,1,2,,true");
    shouldBe(catchParameter([1, , 3]), "1,d,3");
    globalArray = [1, 2, 3];
    shouldBe(viaGlobal(), "1,2,3|xy");
    shouldBe(arrowWithRest([1, , 3]), "1,d,3");
}
