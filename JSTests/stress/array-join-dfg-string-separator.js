function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function joinComma(arr) {
    return arr.join(",");
}
noInline(joinComma);

function joinDash(arr) {
    return arr.join("-");
}
noInline(joinDash);

function joinMulti(arr) {
    return arr.join("--");
}
noInline(joinMulti);

var int32Array = [1, 2, 3, 4, 5];
var contiguousArray = ["a", "b", "c"];
var doubleArray = [1.5, 2.5, 3.5];

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(joinComma(int32Array), "1,2,3,4,5");
    shouldBe(joinDash(int32Array), "1-2-3-4-5");
    shouldBe(joinMulti(int32Array), "1--2--3--4--5");
    shouldBe(joinComma(contiguousArray), "a,b,c");
    shouldBe(joinDash(contiguousArray), "a-b-c");
    shouldBe(joinComma(doubleArray), "1.5,2.5,3.5");
}

// Edge cases
shouldBe(joinComma([]), "");
shouldBe(joinComma([42]), "42");
shouldBe(joinComma([null, undefined, 1]), ",,1");
shouldBe(joinComma([NaN, Infinity, -0]), "NaN,Infinity,0");
shouldBe([1, , 3].join(","), "1,,3");
