var length = 16384;

var source = new Float64Array(length);
for (var i = 0; i < length; ++i)
    source[i] = i * 0.5;

// Keep the input sorted every iteration; sorting it again must stay cheap.
var array = new Float64Array(length);
for (var i = 0; i < 7500; ++i) {
    array.set(source);
    array.sort();
}
