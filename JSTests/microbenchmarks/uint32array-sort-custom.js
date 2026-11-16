var array = new Uint32Array(128);
for (var i = 0; i < array.length; ++i)
    array[i] = 128 - i;

for (var i = 0; i < 1e4; ++i) {
    array.sort(function (a, b) { return a - b; });
    array.sort(function (a, b) { return b - a; });
}
