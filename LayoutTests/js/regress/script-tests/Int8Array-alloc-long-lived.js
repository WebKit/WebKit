//@ skip

var array = new Array(100000);

for (var i = 0; i < 2000000; ++i)
    array[i % array.length] = new Int8Array(9);

for (var i = 0; i < array.length; ++i) {
    var subArray = array[i];
    if (subArray.length != 9)
        throw "Error: bad array length: " + subArray.length;
    for (var j = 0; j < subArray.length; ++j) {
        if (subArray[j] != 0)
            throw "Error: array at " + i + " has non-zero element: " + Array.prototype.join.call(subArray, ",");
    }
}
