//@ skip

var array = new Array(1000);

var n = 10000;

for (var i = 0; i < n; ++i) {
    array[(i / (n / array.length)) | 0] = new Int32Array(100000);
}

for (var i = 0; i < array.length; ++i) {
    if (array[i].length != 100000)
        throw "Error: bad array length: " + array[i].length;
}
