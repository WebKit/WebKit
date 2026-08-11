// Microbenchmark: fast iteration over array.keys()/values()/entries().

function arrayValues(arr) {
    var sum = 0;
    for (var v of arr.values())
        sum += v;
    return sum;
}
noInline(arrayValues);

function arrayKeys(arr) {
    var sum = 0;
    for (var k of arr.keys())
        sum += k;
    return sum;
}
noInline(arrayKeys);

function arrayEntries(arr) {
    var sum = 0;
    for (var [k, v] of arr.entries())
        sum += k + v;
    return sum;
}
noInline(arrayEntries);

var arr = [];
for (var i = 0; i < 1024; ++i)
    arr.push(i);

var iters = 1e4;
for (var i = 0; i < iters; ++i) {
    arrayValues(arr);
    arrayKeys(arr);
    arrayEntries(arr);
}
