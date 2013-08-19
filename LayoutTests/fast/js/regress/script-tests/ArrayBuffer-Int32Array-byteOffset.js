var result = 0;
var buffer = new ArrayBuffer(10);
var array1 = new Int32Array(buffer, 4, 1);
var array2 = new Uint32Array(10);

for (var i = 0; i < 1000000; ++i) {
    result += array1.byteOffset;
    result += array2.byteOffset;
}

if (result != 4000000)
    throw "Error: wrong result: " + result;
