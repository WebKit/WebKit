function shouldThrow(func, expectedError) {
    var errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.toString() !== expectedError)
            throw new Error(`Bad error: ${error}!`);
    }
    if (!errorThrown)
        throw new Error("Not thrown!");
}

function testException(func, expectedError) {
    for (const TA of [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array]) {
        for (let i = 0; i < 1e4; i++)
            shouldThrow(() => func(TA), expectedError);
    }
}

testException(TA => {
    let ta = new TA(4);
    transferArrayBuffer(ta.buffer);
    Object.defineProperty(ta, "0", {value: 1});
}, "TypeError: Underlying ArrayBuffer has been detached from the view or out-of-bounds");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "10", {value: 1});
}, "TypeError: Attempting to store out-of-bounds property on a typed array at index: 10");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "1", {get() {}});
}, "TypeError: Attempting to store accessor property on a typed array at index: 1");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "1", {set(_v) {}});
}, "TypeError: Attempting to store accessor property on a typed array at index: 1");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "2", {configurable: false});
}, "TypeError: Attempting to store non-configurable property on a typed array at index: 2");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "3", {enumerable: false});
}, "TypeError: Attempting to store non-enumerable property on a typed array at index: 3");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "0", {writable: false});
}, "TypeError: Attempting to store non-writable property on a typed array at index: 0");

const myError = new Error("foo");
testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "1", {
        value: { valueOf() { throw myError; } },
    });
}, myError.toString());

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "-1", {value: 1});
}, "TypeError: Attempting to store canonical numeric string property on a typed array");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "0.5", {value: 1});
}, "TypeError: Attempting to store canonical numeric string property on a typed array");

testException(TA => {
    let ta = new TA(4);
    Object.defineProperty(ta, "NaN", {value: 1});
}, "TypeError: Attempting to store canonical numeric string property on a typed array");
