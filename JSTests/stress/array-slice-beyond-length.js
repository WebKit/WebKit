function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

(function sourceIsJSArray() {
    for (var i = 0; i < 10_000; i++) {
        var sourceObj = [0, 1, 2];
        var slicedArr = sourceObj.slice(0, 1000);

        shouldBe(slicedArr.length, 3);
        shouldBe(slicedArr.join(), "0,1,2");
    }
})();

const MAX_ARRAY_LENGTH = 2 ** 32 - 1;

(function sourceIsFinalObject() {
    for (var i = 0; i < 10_000; i++) {
        var sourceObj = {};
        sourceObj[0] = "x";
        sourceObj[MAX_ARRAY_LENGTH] = "y";
        sourceObj.length = MAX_ARRAY_LENGTH + 1;
        sourceObj.slice = Array.prototype.slice;
        var slicedArr = sourceObj.slice(MAX_ARRAY_LENGTH, MAX_ARRAY_LENGTH + 2);

        shouldBe(slicedArr.length, 1);
        shouldBe(slicedArr[0], "y");
    }
})();
