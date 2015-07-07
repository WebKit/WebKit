var result = 0;
function test1(a) {
    result << 1;
    result++;
    return true;
}
function test2(a,b) {
    result ^= 3;
    result *= 3;
    return true;
}
function test3(a,b,c) {
    result ^= result >> 1;
    return true;
}

var result = 0;
var array = []
for (var i = 0; i < 100000; ++i)
    array[i] = 1;

for (var i = 0; i < 10; i++) {
    array.map(test1);
    array.map(test2);
    array.map(test3);
}

if (result != 1428810496)
    throw "Error: bad result: " + result;
