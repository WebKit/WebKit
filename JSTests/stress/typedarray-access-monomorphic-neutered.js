typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];


function check(array, thunk, count) {
    let failed = true;
    try {
        thunk(array, count);
    } catch(e) {
        if (e != "TypeError: Underlying ArrayBuffer has been detached from the view")
            throw new Error([thunk, count, e]);
        failed = false;
    }
    if (failed)
        throw new Error([thunk, count]);
}
noInline(check);

function test(thunk, array) {
    let fn = Function("array", "i", thunk);
    noInline(fn);
    for (let i = 0; i < 10000; i++)
        check(array, fn, i);
}

for (let constructor of typedArrays) {
    let array = new constructor(10);
    transferArrayBuffer(array.buffer);
    test("array[0]", array);
    test("delete array[0]", array);
    test("Object.getOwnPropertyDescriptor(array, 0)", array);
    test("Object.defineProperty(array, 0, { value: 1, writable: true, configurable: false, enumerable: true })", array);
    test("array[0] = 1", array);
    test("array[i] = 1", array);
}

function testFTL(thunk, array, failArray) {
    let fn = Function("array", "i", thunk);
    noInline(fn);
    for (let i = 0; i < 10000; i++)
        fn(array, i)
    check(failArray, fn, 10000);
}

for (let constructor of typedArrays) {
    let array = new constructor(10);
    let failArray = new constructor(10);
    transferArrayBuffer(failArray.buffer);
    testFTL("array[0]", array, failArray);
    testFTL("delete array[0]", array, failArray);
    testFTL("Object.getOwnPropertyDescriptor(array, 0)", array, failArray);
    testFTL("Object.defineProperty(array, 0, { value: 1, writable: true, configurable: false, enumerable: true })", array, failArray);
    testFTL("array[0] = 1", array, failArray);
    testFTL("array[i] = 1", array, failArray);
}
