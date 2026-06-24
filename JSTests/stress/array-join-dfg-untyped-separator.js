function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function shouldThrow(fn, errorType) {
    var threw = false;
    try {
        fn();
    } catch (e) {
        threw = true;
        if (!(e instanceof errorType))
            throw new Error('Expected ' + errorType.name + ', got ' + e);
    }
    if (!threw)
        throw new Error('Expected throw');
}

function join(arr, sep) {
    return arr.join(sep);
}
noInline(join);

var array = [1, 2, 3];

// Number separator -> ToString -> "1"
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(join(array, 1), "11213");

// Boolean separator -> "true"
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(join(array, true), "1true2true3");

// Null separator -> "null"
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(join(array, null), "1null2null3");

// Object with custom toString
var customSep = { toString() { return "<>"; } };
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(join(array, customSep), "1<>2<>3");

// Toggling separator types in the same compiled function (forces UntypedUse)
function pick(i) {
    if (i & 1)
        return ",";
    return 1;
}
function joinPicked(arr, i) {
    return arr.join(pick(i));
}
noInline(joinPicked);
for (var i = 0; i < testLoopCount; ++i) {
    if (i & 1)
        shouldBe(joinPicked(array, i), "1,2,3");
    else
        shouldBe(joinPicked(array, i), "11213");
}

// Symbol separator throws TypeError
for (var i = 0; i < 1000; ++i) {
    shouldThrow(() => join(array, Symbol("s")), TypeError);
}

// toString that throws propagates the error
function throwingSep() {
    return { toString() { throw new Error("boom"); } };
}
for (var i = 0; i < 1000; ++i) {
    var threw = false;
    try {
        join(array, throwingSep());
    } catch (e) {
        threw = true;
        if (e.message !== "boom")
            throw new Error('Wrong error: ' + e.message);
    }
    if (!threw)
        throw new Error('Expected throw');
}
