var length = 192;

var seed = 1;
function nextRandom() {
    seed ^= seed << 13; seed |= 0;
    seed ^= seed >>> 17;
    seed ^= seed << 5; seed |= 0;
    return seed >>> 24;
}

var source = new Uint8Array(length);
for (var i = 0; i < length; ++i)
    source[i] = nextRandom();

// Restore the unsorted input every iteration so the sort never sees an already sorted array.
var array = new Uint8Array(length);
for (var i = 0; i < 300000; ++i) {
    array.set(source);
    array.sort();
}
