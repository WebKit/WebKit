//@ skip if not $jitTests
//@ runDefault("--validateOptions=true", "--useConcurrentJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotBeCompiledRepeatedly(func) {
    if (numberOfDFGCompiles(func) > 4)
        throw new Error(`${func.name} was compiled ${numberOfDFGCompiles(func)} times`);
}

let object = { };
let count = testLoopCount * 10;

function putAfterOptimized(array, index, value) {
    array[index] = value;
}
noInline(putAfterOptimized);

function putOncePerArray(array, index, value) {
    array[index] = value;
}
noInline(putOncePerArray);

function putToArrayBecomingSparse(array, index, value) {
    array[index] = value;
}
noInline(putToArrayBecomingSparse);

function read(array, index) {
    return array[index];
}
noInline(read);

for (let i = 0; i < count; ++i) {
    putAfterOptimized([1, 2, 3], 1, i);
    putAfterOptimized([object, object], 1, object);
}
for (let i = 0; i < count; ++i) {
    let array = [];
    putAfterOptimized(array, 100008 + i, 3);
    shouldBe(array[100008 + i], 3);
    shouldBe(array.length, 100009 + i);
}
shouldNotBeCompiledRepeatedly(putAfterOptimized);

for (let i = 0; i < count; ++i) {
    let array = [];
    putOncePerArray(array, 100008 + i, 3);
    shouldBe(array[100008 + i], 3);
    shouldBe(array.length, 100009 + i);
}
shouldNotBeCompiledRepeatedly(putOncePerArray);

for (let i = 0; i < count; ++i) {
    let array = [];
    putToArrayBecomingSparse(array, 0, 1);
    putToArrayBecomingSparse(array, 1, 2);
    putToArrayBecomingSparse(array, 100008, 3);
    putToArrayBecomingSparse(array, 200011, 4);
    shouldBe(array[0] + array[1] + array[100008] + array[200011], 10);
    shouldBe(array.length, 200012);
}
shouldNotBeCompiledRepeatedly(putToArrayBecomingSparse);

let contiguous = [object, object, object];
for (let i = 0; i < count; ++i) {
    shouldBe(read(contiguous, 1), object);
    shouldBe(read([1, 2, 3], (i % 10) ? 1 : 200000), (i % 10) ? 2 : undefined);
}
shouldNotBeCompiledRepeatedly(read);
