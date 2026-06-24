function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function joinInt32(arr) {
    return arr.join("");
}
noInline(joinInt32);

function joinContiguous(arr) {
    return arr.join("");
}
noInline(joinContiguous);

function joinDouble(arr) {
    return arr.join("");
}
noInline(joinDouble);

var int32Array = [1, 2, 3, 4, 5];
var contiguousArray = ["a", "b", "c", "d", "e"];
var doubleArray = [1.5, 2.5, 3.5];
var mixedArray = [1, "x", 2, "y", 3];

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(joinInt32(int32Array), "12345");
    shouldBe(joinContiguous(contiguousArray), "abcde");
    shouldBe(joinDouble(doubleArray), "1.52.53.5");
    shouldBe(joinContiguous(mixedArray), "1x2y3");
}

shouldBe(joinInt32([]), "");
shouldBe(joinInt32([42]), "42");
shouldBe(joinContiguous(["solo"]), "solo");

var withHole = [1, , 3];
shouldBe(withHole.join(""), "13");

var sparseInt32 = [1, 2];
sparseInt32.length = 4;
shouldBe(sparseInt32.join(""), "12");
