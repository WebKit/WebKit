function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

// Parser-level fold: arr.join(undefined) where undefined is a static JSConstant
function joinUndefinedLiteral(arr) {
    return arr.join(undefined);
}
noInline(joinUndefinedLiteral);

// Parser-level fold: arr.join() with no argument (separator is implicit undefined)
function joinNoArg(arr) {
    return arr.join();
}
noInline(joinNoArg);

// Strength-reduction case: separator is a variable that resolves to undefined
// after constant propagation. The bytecode parser sees a non-constant Get,
// but later phases fold it.
function joinViaVar(arr) {
    var sep;
    return arr.join(sep);
}
noInline(joinViaVar);

// Inlined: callee passes through a parameter that the caller leaves undefined.
function inner(arr, sep) {
    return arr.join(sep);
}
function outer(arr) {
    return inner(arr);
}
noInline(outer);

var int32Array = [1, 2, 3, 4, 5];
var contiguousArray = ["a", "b", "c"];
var doubleArray = [1.5, 2.5, 3.5];

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(joinUndefinedLiteral(int32Array), "1,2,3,4,5");
    shouldBe(joinUndefinedLiteral(contiguousArray), "a,b,c");
    shouldBe(joinUndefinedLiteral(doubleArray), "1.5,2.5,3.5");

    shouldBe(joinNoArg(int32Array), "1,2,3,4,5");
    shouldBe(joinNoArg(contiguousArray), "a,b,c");

    shouldBe(joinViaVar(int32Array), "1,2,3,4,5");
    shouldBe(joinViaVar(contiguousArray), "a,b,c");

    shouldBe(outer(int32Array), "1,2,3,4,5");
    shouldBe(outer(contiguousArray), "a,b,c");
}
