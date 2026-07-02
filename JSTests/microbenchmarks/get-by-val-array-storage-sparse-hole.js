//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// Hole/miss-dominated reads of a large sparse array (ArrayStorage mode with a sparse map). Every read
// hits an in-bounds hole that is absent from the sparse map, so the int-indexed GetByVal slow path
// (operationGetByValArrayStorageInt) must resolve it to undefined via the (sane) prototype chain.
function get(array, i)
{
    return array[i];
}
noInline(get);

var maxIndex = 200000;
var step = 16;

var array = [];
for (var i = maxIndex - step; i >= 0; i -= step)
    array[i] = i + 1;

var expectedPass = 0;
for (var i = 1; i < maxIndex; i += step) // i = 1, 17, 33, ... are all holes (never written).
    ++expectedPass;

var iterations = 400;
var holes = 0;
for (var iter = 0; iter < iterations; ++iter) {
    for (var i = 1; i < maxIndex; i += step) {
        if (get(array, i) === void 0)
            ++holes;
    }
}

if (holes !== expectedPass * iterations)
    throw "Error: bad hole count: " + holes;
