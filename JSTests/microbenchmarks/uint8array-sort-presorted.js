var length = 4096;

var source = new Uint8Array(length);
for (var i = 0; i < length; ++i)
    source[i] = (i * 256 / length) | 0;

// Keep the input sorted every iteration; sorting it again must stay cheap.
var array = new Uint8Array(length);
for (var i = 0; i < 60000; ++i) {
    array.set(source);
    array.sort();
}
