var array = new Array(100000);

for (var i = 0; i < 2000000; ++i)
    array[i % array.length] = new Int32Array(10);

for (var i = 0; i < array.length; ++i) {
    if (array[i].length != 10)
        throw "Error: bad array length: " + array[i].length;
}
