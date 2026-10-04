function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function store(array, index, value) {
    array[index] = value;
}

var values = [0, 1, -1, 127, 128, -128, -129, 255, 256, 32767, 32768, -32769, 65535, 65536, 0x7fffffff, -0x80000000, 1.5, -0.5, 300.5, NaN, "7", undefined];
var constructors = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

for (var i = 0; i < testLoopCount / 8; ++i) {
    var index = i & 3;
    for (var constructor of constructors) {
        var array = new constructor(4);
        var expected = new constructor(4);
        for (var value of values) {
            store(array, index, value);
            Reflect.set(expected, index, value);
            shouldBe(array[index], expected[index]);
        }
        store(array, 4, 42);
        shouldBe(array[4], undefined);
        store(array, -1, 42);
        shouldBe(array[-1], undefined);
    }
}

var resizable = new Uint8Array(new ArrayBuffer(4, { maxByteLength: 8 }));
for (var i = 0; i < testLoopCount; ++i) {
    store(resizable, i & 3, 300);
    shouldBe(resizable[i & 3], 44);
}
resizable.buffer.resize(2);
store(resizable, 3, 1);
shouldBe(resizable[3], undefined);

var detached = new Int16Array(4);
transferArrayBuffer(detached.buffer);
store(detached, 0, 1);
shouldBe(detached[0], undefined);
