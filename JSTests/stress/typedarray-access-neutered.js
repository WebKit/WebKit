//@ skip if $architecture == "mips"

typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];


function check(thunk, count) {
    let array = new constructor(10);
    transferArrayBuffer(array.buffer);
    let failed = true;
    try {
        thunk(array);
    } catch(e) {
        if (e != "TypeError: Underlying ArrayBuffer has been detached from the view")
            throw new Error([thunk, count, e]);
        failed = false;
    }
    if (failed)
        throw new Error([thunk, count]);
}

function test(thunk, count) {
    for (constructor of typedArrays)
        check(thunk, count);
}

for (let i = 0; i < 10000; i++) {
    test((array) => array[0], i);
    test((array) => delete array[0], i);
    test((array) => Object.getOwnPropertyDescriptor(array, 0), i);
    test((array) => array[0] = 1, i);
}

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
    testNoException((array) => Object.defineProperty(array, 0, { value: 1, writable: true, configurable: false, enumerable: true }), i)
}
