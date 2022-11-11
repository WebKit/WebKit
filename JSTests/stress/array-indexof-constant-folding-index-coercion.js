function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function() {
    const int32Array = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
    const doubleArray = [0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5];

    const indexOfInt32Cell = index => int32Array.indexOf("test", index);
    const indexOfInt32Other = index => int32Array.indexOf(null, index);
    const indexOfInt32Boolean = index => int32Array.indexOf(true, index);
    const indexOfDoubleCell = index => doubleArray.indexOf(Symbol(), index);
    const indexOfDoubleOther = index => doubleArray.indexOf(undefined, index);
    const indexOfDoubleBoolean = index => doubleArray.indexOf(false, index);

    noInline(indexOfInt32Cell);
    noInline(indexOfInt32Other);
    noInline(indexOfInt32Boolean);
    noInline(indexOfDoubleCell);
    noInline(indexOfDoubleOther);
    noInline(indexOfDoubleBoolean);

    for (var i = 0; i < 1e6; ++i) {
        shouldBe(indexOfInt32Cell(0), -1);
        shouldBe(indexOfInt32Other(0), -1);
        shouldBe(indexOfInt32Boolean(0), -1);
        shouldBe(indexOfDoubleCell(0), -1);
        shouldBe(indexOfDoubleOther(0), -1);
        shouldBe(indexOfDoubleBoolean(0), -1);
    }

    let coercibleIndexCalls = 0;
    const coercibleIndex = {
        valueOf: () => {
            coercibleIndexCalls++;
            return 0;
        },
    };

    shouldBe(indexOfInt32Cell(coercibleIndex), -1);
    shouldBe(indexOfInt32Other(coercibleIndex), -1);
    shouldBe(indexOfInt32Boolean(coercibleIndex), -1);
    shouldBe(indexOfDoubleCell(coercibleIndex), -1);
    shouldBe(indexOfDoubleOther(coercibleIndex), -1);
    shouldBe(indexOfDoubleBoolean(coercibleIndex), -1);

    shouldBe(coercibleIndexCalls, 6);
})();
