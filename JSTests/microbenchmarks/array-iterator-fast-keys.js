// Microbenchmark: fast iteration over array.keys() (no element load needed).

function arrayKeysSum(arr) {
    var sum = 0;
    for (var k of arr.keys())
        sum += k;
    return sum;
}
noInline(arrayKeysSum);

var arr = [];
for (var i = 0; i < 1024; ++i)
    arr.push(i);

var iters = 4e4;
for (var i = 0; i < iters; ++i)
    arrayKeysSum(arr);
