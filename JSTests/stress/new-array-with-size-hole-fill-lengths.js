function assert(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function makeDouble(n) {
    let array = new Array(n);
    array[0] = 0.5;
    return array;
}
noInline(makeDouble);

function makeContiguous(n) {
    let array = new Array(n);
    array[0] = "x";
    return array;
}
noInline(makeContiguous);

function checkHoles(array, n) {
    assert(array.length, n);
    for (let i = 1; i < n; ++i) {
        assert(i in array, false);
        assert(array[i], undefined);
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    for (let n = 1; n < 40; ++n) {
        let doubles = makeDouble(n);
        assert(doubles[0], 0.5);
        checkHoles(doubles, n);
        let values = makeContiguous(n);
        assert(values[0], "x");
        checkHoles(values, n);
    }
}
