typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

function checkNoException(thunk, count) {
    let array = new constructor(10);
    transferArrayBuffer(array.buffer);
    thunk(array);
}

function testNoException(thunk, count) {
    for (constructor of typedArrays)
        checkNoException(thunk, count);
}

for (let i = 0; i < 10000; i++) {
    testNoException((array) => array[0], i);
    testNoException((array) => delete array[0], i);
    testNoException((array) => Object.getOwnPropertyDescriptor(array, 0), i);
    testNoException((array) => array[0] = 1, i);
}
