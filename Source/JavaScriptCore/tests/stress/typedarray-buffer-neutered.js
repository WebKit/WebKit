typedArrays = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float32Array, Float64Array];

buffer = new ArrayBuffer(16);
transferArrayBuffer(buffer);

arrays = typedArrays.map(function(constructor) { return new constructor(buffer); });

for (i = 0; i < 100000; i++) {
    arrays.forEach(function(array) {
        if (array.buffer !== buffer)
            throw "wrong buffer";
    });
}
