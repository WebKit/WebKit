var length = 64;

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

// Short enough to stay on the comparison sort, which guards the counting sort threshold.
var array = new Uint8Array(length);
for (var i = 0; i < 500000; ++i) {
    array.set(source);
    array.sort();
}
