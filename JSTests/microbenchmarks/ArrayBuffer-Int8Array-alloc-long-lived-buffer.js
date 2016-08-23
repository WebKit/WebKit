//@ skip

var array = new Array(10000);

for (var i = 0; i < 100000; ++i)
    array[i % array.length] = new Int8Array(new ArrayBuffer(10)).buffer;

for (var i = 0; i < array.length; ++i) {
    if (array[i].byteLength != 10)
        throw "Error: bad byteLength: " + array[i].byteLength;
}
