//@ runDefault("--useFTLJIT=0")

// Exercises the OOB JSArray store path of MultiPutByVal in DFG, which extends
// publicLength when the index is past it but still within vectorLength, and
// falls into the operationPutByValBeyondArrayBounds slow path otherwise.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function test(array, index, value) {
    array[index] = value;
    return array;
}
noInline(test);

// Warm up with both Int32 and Contiguous arrays plus OOB stores so the DFG
// chooses MultiPutByVal with isOutOfBounds().
let warmInt32 = [0, 0, 0];
let warmContig = ["a", "b", "c"];
for (let i = 0; i < testLoopCount; ++i) {
    test(warmInt32, 0, 42);
    test(warmContig, 0, "x");
    test(warmInt32, 5, 7); // past publicLength -> exit site
    test(warmContig, 5, "y");
}

for (let i = 0; i < testLoopCount; ++i) {
    let arr = [1, 2, 3];
    test(arr, 0, 11); // in bounds
    shouldBeArray(arr, [11, 2, 3]);

    test(arr, 3, 99); // grow by one (still likely in vectorLength)
    shouldBeArray(arr, [11, 2, 3, 99]);

    test(arr, 100, 7); // far OOB -> slow path
    shouldBe(arr[100], 7);
    shouldBe(arr.length, 101);

    let cont = ["a", "b"];
    test(cont, 1, "z");
    shouldBe(cont[1], "z");
    test(cont, 4, "q");
    shouldBe(cont[4], "q");
    shouldBe(cont.length, 5);
}
