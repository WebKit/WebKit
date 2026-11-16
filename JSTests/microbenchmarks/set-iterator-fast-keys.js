// Microbenchmark: fast iteration over set.keys() (== set.values() in JS, but exercises the
// FastSetValues reuse path with an iterator obtained via .keys()).

function setKeysSum(set) {
    var sum = 0;
    for (var k of set.keys())
        sum += k;
    return sum;
}
noInline(setKeysSum);

var set = new Set();
for (var i = 0; i < 1024; ++i)
    set.add(i);

var iters = 2e4;
for (var i = 0; i < iters; ++i)
    setKeysSum(set);
