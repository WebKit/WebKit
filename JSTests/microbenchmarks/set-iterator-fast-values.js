// Microbenchmark: fast iteration over set.values() / for-of set.

function setValuesSum(set) {
    var sum = 0;
    for (var v of set.values())
        sum += v;
    return sum;
}
noInline(setValuesSum);

function setForOfSum(set) {
    var sum = 0;
    for (var v of set)
        sum += v;
    return sum;
}
noInline(setForOfSum);

var set = new Set();
for (var i = 0; i < 1024; ++i)
    set.add(i);

var iters = 2e4;
for (var i = 0; i < iters; ++i) {
    setValuesSum(set);
    setForOfSum(set);
}
