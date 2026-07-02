// Microbenchmark: fast iteration over set.entries() (allocates [value, value] per step).

function setEntriesSum(set) {
    var sum = 0;
    for (var [a, b] of set.entries())
        sum += a + b;
    return sum;
}
noInline(setEntriesSum);

var set = new Set();
for (var i = 0; i < 1024; ++i)
    set.add(i);

var iters = 1e4;
for (var i = 0; i < iters; ++i)
    setEntriesSum(set);
