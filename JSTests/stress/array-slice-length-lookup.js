function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Bad value: ${actual}!`);
}

(function sourceIsFinalObject() {
    var lengthLookups = 0;
    var sourceObj = {
        0: 0, 1: 1, 2: 2,
        get length() {
            lengthLookups++;
            return 3;
        },
    };

    var slicedArr;
    for (var i = 0; i < 1e6; i++) {
        slicedArr = [].slice.call(sourceObj);
    }

    shouldBe(slicedArr.length, 3);
    shouldBe(lengthLookups, 1e6);
})();
