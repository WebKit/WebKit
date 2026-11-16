//@ runDefault("--useFTLJIT=0")

// Exercises the OutOfBoundsSaneChain path of MultiGetByVal in DFG.
// Sane chain reads return undefined for OOB without going through the slow path,
// after speculating the index is non-negative.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}
noInline(shouldBe);

function test(array, index) {
    return array[index];
}
noInline(test);

let jsInt32 = [10, 20, 30];
let jsDouble = [1.5, 2.5, 3.5];
let jsContiguous = ["a", "b", "c"];

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(test(jsInt32, 0), 10);
    shouldBe(test(jsInt32, 2), 30);
    shouldBe(test(jsInt32, 3), undefined);
    shouldBe(test(jsInt32, 100), undefined);

    shouldBe(test(jsDouble, 0), 1.5);
    shouldBe(test(jsDouble, 2), 3.5);
    shouldBe(test(jsDouble, 3), undefined);
    shouldBe(test(jsDouble, 100), undefined);

    shouldBe(test(jsContiguous, 0), "a");
    shouldBe(test(jsContiguous, 2), "c");
    shouldBe(test(jsContiguous, 3), undefined);
    shouldBe(test(jsContiguous, 100), undefined);
}
