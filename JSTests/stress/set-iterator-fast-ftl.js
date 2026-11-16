// FTL fast path for set.values()/entries() iteration.
//@ runFTLNoCJIT

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(set) {
    var sum = 0;
    for (var v of set.values()) sum += v;
    return sum;
}

function entries(set) {
    var sum = 0;
    for (var [a, b] of set.entries()) sum += a + b;
    return sum;
}

function plain(set) {
    var sum = 0;
    for (var v of set) sum += v;
    return sum;
}

var set = new Set();
for (var i = 0; i < 10; ++i)
    set.add(i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(set), 55);
    shouldBe(entries(set), 110);
    shouldBe(plain(set), 55);
}
