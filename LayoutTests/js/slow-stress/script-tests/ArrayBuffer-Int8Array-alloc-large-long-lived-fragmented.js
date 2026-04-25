//@ skip

var array = new Array(10000);

var fragmentedArray = [];

for (var i = 0; i < 100000; ++i) {
    var newArray = new Int8Array(new ArrayBuffer(1000));
    if (i % 10 === 0)
        newArray.customProperty = "foo";
    for (var j = 0; j < 10; j++)
        fragmentedArray = new Array(10);
    array[i % array.length] = newArray;
}

for (var i = 0; i < array.length; ++i) {
    if (array[i].length != 1000)
        throw "Error: bad length: " + array[i].length;
    if (array[i].buffer.byteLength != 1000)
        throw "Error: bad buffer.byteLength: " + array[i].buffer.byteLength;
}
