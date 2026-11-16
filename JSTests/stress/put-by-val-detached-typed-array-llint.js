//@ runDefault("--useJIT=0")

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function store(array, index, value) {
    array[index] = value;
}

var constructors = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array];

for (var constructor of constructors) {
    var array = new constructor(new ArrayBuffer(8 * constructor.BYTES_PER_ELEMENT));
    for (var i = 0; i < 100; ++i) {
        if (i === 50) {
            transferArrayBuffer(array.buffer);
            shouldBe(array.length, 0);
        }
        var index = i & 7;
        store(array, index, 1);
        shouldBe(array[index], i < 50 ? 1 : undefined);
    }
    store(array, -1, 1);
    store(array, 0x7fffffff, 1);
    shouldBe(array.length, 0);

    var valueOfCalls = 0;
    store(array, 0, { valueOf() { ++valueOfCalls; return 1; } });
    shouldBe(valueOfCalls, 1);
    shouldBe(array[0], undefined);
}
