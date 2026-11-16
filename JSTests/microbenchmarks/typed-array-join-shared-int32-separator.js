var seed = 42;
function next() {
    seed = (seed + 0x9e3779b9) | 0;
    var t = seed ^ (seed >>> 16);
    t = Math.imul(t, 0x21f0aaad);
    t = t ^ (t >>> 15);
    t = Math.imul(t, 0x735a2d97);
    return t ^ (t >>> 15);
}

var array = new Int32Array(new SharedArrayBuffer(4 << 14));
for (var i = 0; i < array.length; ++i)
    array[i] = next();

function test(array) {
    return array.join(", ");
}
noInline(test);

var expected = test(array).length;
var total = 0;
for (var i = 0; i < 200; ++i)
    total += test(array).length;
if (total !== expected * 200)
    throw new Error("bad result: " + total);
