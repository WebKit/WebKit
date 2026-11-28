function sum(array) {
    var result = 0;
    for (var value of array)
        result += value;
    return result;
}
noInline(sum);

var arrays = [new Int32Array(1024), new Float64Array(1024), new Uint8Array(1024)];
for (var array of arrays) {
    for (var i = 0; i < array.length; ++i)
        array[i] = i & 0x7f;
}

for (var i = 0; i < 2e4; ++i) {
    if (sum(arrays[i % arrays.length]) !== 65024)
        throw new Error("bad sum");
}
