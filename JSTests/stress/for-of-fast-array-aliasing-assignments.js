// While an Array is destructured or looped over without an iterator object, the Array is read back from a register at
// every step. Nothing the program does to the binding the Array came from (parameters through `arguments`, eval, with,
// closures, assignments in default values and in the loop body) may change which Array is being iterated.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const other = ["x", "y", "z"];
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

for (var i = 0; i < testLoopCount; i++) {
    shouldBe(viaClosure([1, , 3]), "1,d,3|7,8,9");
    shouldBe(selfAssign([1, 2, 3]), "2");
    shouldBe(selfAssignLoop([1, 2, 3]), "1,2,3|3");
    shouldBe(nestedSame([[1, 2], , ,]), "1,2,1,2,,true");
    shouldBe(catchParameter([1, , 3]), "1,d,3");
    globalArray = [1, 2, 3];
    shouldBe(viaGlobal(), "1,2,3|xy");
}
