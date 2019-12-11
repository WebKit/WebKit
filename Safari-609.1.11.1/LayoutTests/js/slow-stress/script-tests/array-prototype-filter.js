var result = 0;
function test1(a) {
    result << 1;
    result++;
    return true;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return (result & 1) == 0;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return true;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

var totalLength = 0
for (var i = 0; i < 10; i++) {
    totalLength += array.filter(test1).length;
    totalLength += array.filter(test2).length;
    totalLength += array.filter(test3).length;
}

if (result != 1428810496)
    throw "Error: bad result: " + result;

if (totalLength != 2500000)
    throw "Error: bad length: " + totalLength;