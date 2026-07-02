//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// Hit-dominated reads of a large sparse array (ArrayStorage mode with a sparse map). Every read
// resolves through the int-indexed GetByVal slow path (operationGetByValArrayStorageInt) and finds
// its value in the sparse map.
function get(array, i)
{
    return array[i];
}
noInline(get);

var maxIndex = 200000;
var step = 16; // 1/16 density (< 1/8) keeps the array in ArrayStorage with a sparse map.

var array = [];
// Descending writes: the first one is far beyond length, forcing ArrayStorage + sparse map.
for (var i = maxIndex - step; i >= 0; i -= step)
    array[i] = i + 1;

var expectedPass = 0;
for (var i = 0; i < maxIndex; i += step)
    expectedPass += i + 1;

var iterations = 800;
var sum = 0;
for (var iter = 0; iter < iterations; ++iter) {
    for (var i = 0; i < maxIndex; i += step)
        sum += get(array, i);
}

if (sum !== expectedPass * iterations)
    throw "Error: bad sum: " + sum;
