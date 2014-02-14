var result = 0;
function test1(a) {
    result << 1;
    result++;
    return false;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return false;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return false;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.some(test1);
    array.some(test2);
    array.some(test3);
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
