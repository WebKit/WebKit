// One function per indexing type, to keep each unshift / shift monomorphic.
function testContiguous(array, value) {
    array.unshift(value);
    return array.shift();
}
noInline(testContiguous);

function testInt32(array, value) {
    array.unshift(value);
    return array.shift();
}
noInline(testInt32);

function testDouble(array, value) {
    array.unshift(value);
    return array.shift();
}
noInline(testDouble);

// Leave spare capacity so that unshift does not have to grow the butterfly.
function withSpareCapacity(array) {
    array.push(array[0]);
    array.pop();
    return array;
}

const object = { x: 1 };
const contiguous = withSpareCapacity([object, "a", object, "b", object, "c", object, "d"]);
const int32 = withSpareCapacity([1, 2, 3, 4, 5, 6]);
const double = withSpareCapacity([1.5, 2.5, 3.5, 4.5]);

for (let i = 0; i < 1e6; ++i) {
    testContiguous(contiguous, object);
    testInt32(int32, 7);
    testDouble(double, 8.5);
}
