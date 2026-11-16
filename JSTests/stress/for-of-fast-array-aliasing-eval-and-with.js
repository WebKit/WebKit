// While an Array is destructured or looped over without an iterator object, the Array is read back from a register at
// every step. Nothing the program does to the binding the Array came from (parameters through `arguments`, eval, with,
// closures, assignments in default values and in the loop body) may change which Array is being iterated.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const other = ["x", "y", "z"];
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

for (var i = 0; i < testLoopCount; i++) {
    shouldBe(viaEval([1, , 3]), "1,d,3,true");
    shouldBe(viaEvalLoop([1, 2, 3]), "1,2,3");
    shouldBe(viaWith({ p: [1, , 3], q: [4, 5, 6] }), "1,d,3|4,5,6|truetrue");
}
