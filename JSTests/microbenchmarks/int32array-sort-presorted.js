var length = 4096;

var source = new Int32Array(length);
for (var i = 0; i < length; ++i)
    source[i] = i * 1024;

// Keep the input sorted every iteration; sorting it again must stay cheap.
var array = new Int32Array(length);
for (var i = 0; i < 40000; ++i) {
    array.set(source);
    array.sort();
}
