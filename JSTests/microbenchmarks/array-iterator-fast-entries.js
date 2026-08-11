// Microbenchmark: fast iteration over array.entries() (allocates [index, element] per step).

function arrayEntriesSum(arr) {
    var sum = 0;
    for (var [k, v] of arr.entries())
        sum += k + v;
    return sum;
}
noInline(arrayEntriesSum);

var arr = [];
for (var i = 0; i < 1024; ++i)
    arr.push(i);

var iters = 2e4;
for (var i = 0; i < iters; ++i)
    arrayEntriesSum(arr);
