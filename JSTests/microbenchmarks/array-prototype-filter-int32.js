function test(array) {
    return array.filter(x => (x & 1) === 0);
}
noInline(test);

var array = [];
for (var i = 0; i < 64; ++i)
    array.push(i);

var result;
for (var i = 0; i < 1e6; ++i)
    result = test(array);

if (result.length !== 32)
    throw new Error("bad length: " + result.length);
for (var i = 0; i < 32; ++i) {
    if (result[i] !== i * 2)
        throw new Error("bad value at " + i + ": " + result[i]);
}
