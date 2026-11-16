var length = 4096;

var seed = 1;
function nextRandom() {
    seed ^= seed << 13; seed |= 0;
    seed ^= seed >>> 17;
    seed ^= seed << 5; seed |= 0;
    return seed;
}

var source = new Uint32Array(length);
for (var i = 0; i < length; ++i)
    source[i] = nextRandom() >>> 0;

// Restore the unsorted input every iteration so the sort never sees an already sorted array.
var array = new Uint32Array(length);
for (var i = 0; i < 6000; ++i) {
    array.set(source);
    array.sort();
}
