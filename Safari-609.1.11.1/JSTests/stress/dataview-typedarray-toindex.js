function assert(condition) {
    if (!condition)
        throw new Error("Bad assertion");
}

function shouldThrowRangeError(func) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (!(error instanceof RangeError))
        throw new Error(`bad error: ${String(error)}`);
}

// ToIndex (value) https://tc39.github.io/ecma262/#sec-toindex
// - Convert undefined to 0.
// - Convert value to Integer, and throws a RangeError if negative.
// - JavaScriptCore also throws a TypeError for Infinity because that would convert tp 2^53 - 1 which is too large for all cases.

let buffer = new ArrayBuffer(128);
let dataView = new DataView(buffer);    
let rangeErrorValues = [-1, -Infinity, Infinity];
let typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

// ArrayBuffer(length*) https://tc39.github.io/ecma262/#sec-arraybuffer-length
assert((new ArrayBuffer).byteLength === 0);
assert((new ArrayBuffer(undefined)).byteLength === 0);
assert((new ArrayBuffer(-0.9)).byteLength === 0);
assert((new ArrayBuffer(2.5)).byteLength === 2);
for (let badValue of rangeErrorValues)
    shouldThrowRangeError(() => { new ArrayBuffer(badValue); });

// TypedArray(length*) https://tc39.github.io/ecma262/#sec-typedarray-length
// TypedArray(buffer, byteOffset*, length*) https://tc39.github.io/ecma262/#sec-typedarray-buffer-byteoffset-length
for (let typedArray of typedArrays) {
    assert((new typedArray).length === 0);
    assert((new typedArray(2.5)).length === 2);
    assert((new typedArray(-0.9)).length === 0);
    assert((new typedArray(buffer, typedArray.BYTES_PER_ELEMENT + 0.5)).byteOffset === typedArray.BYTES_PER_ELEMENT);
    assert((new typedArray(buffer, 0, typedArray.BYTES_PER_ELEMENT + 0.5)).byteLength / typedArray.BYTES_PER_ELEMENT === typedArray.BYTES_PER_ELEMENT);

    for (let badValue of rangeErrorValues) {
        shouldThrowRangeError(() => { new typedArray(badValue); });
        shouldThrowRangeError(() => { new typedArray(buffer, badValue); });
        shouldThrowRangeError(() => { new typedArray(buffer, 0, badValue); });
    }
}

// DataView(buffer, byteOffset*, byteLength*) https://tc39.github.io/ecma262/#sec-dataview-buffer-byteoffset-bytelength
assert((new DataView(buffer)).byteOffset === 0);
assert((new DataView(buffer, undefined)).byteOffset === 0);
assert((new DataView(buffer, 2.5)).byteOffset === 2);
assert((new DataView(buffer, -0.9)).byteOffset === 0);
assert((new DataView(buffer, 0, 4.5)).byteLength === 4);
for (let badValue of rangeErrorValues) {
    shouldThrowRangeError(() => { new DataView(buffer, badValue); });
    shouldThrowRangeError(() => { new DataView(buffer, 0, badValue); });
}

// GetViewValue(view, index*, ...) https://tc39.github.io/ecma262/#sec-getviewvalue
// SetViewValue(view, index*, ...) https://tc39.github.io/ecma262/#sec-setviewvalue
dataView.setInt8(undefined, 42);
assert(dataView.getInt8(0) === 42);
dataView.setInt16(undefined, 42);
assert(dataView.getInt16(0) === 42);
dataView.setInt32(undefined, 42);
assert(dataView.getInt32(0) === 42);
dataView.setUint8(undefined, 42);
assert(dataView.getUint8(0) === 42);
dataView.setUint16(undefined, 42);
assert(dataView.getUint16(0) === 42);
dataView.setUint32(undefined, 42);
assert(dataView.getUint32(0) === 42);
dataView.setFloat32(undefined, 42);
assert(dataView.getFloat32(0) === 42);
dataView.setFloat64(undefined, 42);
assert(dataView.getFloat64(0) === 42);

dataView.setInt8(2.5, 42);
assert(dataView.getInt8(2) === 42);
dataView.setInt16(2.5, 42);
assert(dataView.getInt16(2) === 42);
dataView.setInt32(2.5, 42);
assert(dataView.getInt32(2) === 42);
dataView.setUint8(2.5, 42);
assert(dataView.getUint8(2) === 42);
dataView.setUint16(2.5, 42);
assert(dataView.getUint16(2) === 42);
dataView.setUint32(2.5, 42);
assert(dataView.getUint32(2) === 42);
dataView.setFloat32(2.5, 42);
assert(dataView.getFloat32(2) === 42);
dataView.setFloat64(2.5, 42);
assert(dataView.getFloat64(2) === 42);

for (let badValue of rangeErrorValues) {
    shouldThrowRangeError(() => { new DataView(buffer, badValue); });
    shouldThrowRangeError(() => { new DataView(buffer, 0, badValue); });
    shouldThrowRangeError(() => { dataView.getInt8(badValue); });
    shouldThrowRangeError(() => { dataView.getInt16(badValue); });
    shouldThrowRangeError(() => { dataView.getInt32(badValue); });
    shouldThrowRangeError(() => { dataView.getUint8(badValue); });
    shouldThrowRangeError(() => { dataView.getUint16(badValue); });
    shouldThrowRangeError(() => { dataView.getUint32(badValue); });
    shouldThrowRangeError(() => { dataView.getFloat32(badValue); });
    shouldThrowRangeError(() => { dataView.getFloat64(badValue); });
    shouldThrowRangeError(() => { dataView.setInt8(badValue, 42); });
    shouldThrowRangeError(() => { dataView.setInt16(badValue, 42); });
    shouldThrowRangeError(() => { dataView.setInt32(badValue, 42); });
    shouldThrowRangeError(() => { dataView.setUint8(badValue, 42); });
    shouldThrowRangeError(() => { dataView.setUint16(badValue, 42); });
    shouldThrowRangeError(() => { dataView.setUint32(badValue, 42); });
    shouldThrowRangeError(() => { dataView.setFloat32(badValue, 42); });
    shouldThrowRangeError(() => { dataView.setFloat64(badValue, 42); });
}
