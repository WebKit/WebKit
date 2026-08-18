var length = 16384;

var seed = 1;
function nextRandom() {
    seed ^= seed << 13; seed |= 0;
    seed ^= seed >>> 17;
    seed ^= seed << 5; seed |= 0;
    return seed;
}

// Two draws per element so the mantissa has entropy in every byte. A single 32-bit draw leaves
// the low mantissa bytes zero, which lets the sort skip digit passes that real data would not.
var source = new Float64Array(length);
for (var i = 0; i < length; ++i)
    source[i] = nextRandom() + nextRandom() * 2.3283064365386963e-10;

// Restore the unsorted input every iteration so the sort never sees an already sorted array.
var array = new Float64Array(length);
for (var i = 0; i < 300; ++i) {
    array.set(source);
    array.sort();
}
