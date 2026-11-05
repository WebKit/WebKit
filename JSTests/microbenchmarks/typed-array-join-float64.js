var seed = 42;
function next() {
    seed = (seed + 0x9e3779b9) | 0;
    var t = seed ^ (seed >>> 16);
    t = Math.imul(t, 0x21f0aaad);
    t = t ^ (t >>> 15);
    t = Math.imul(t, 0x735a2d97);
    return (t ^ (t >>> 15)) >>> 0;
}

var array = new Float64Array(1 << 14);
for (var i = 0; i < array.length; ++i)
    array[i] = next() / 0x100000000 * 1000 - 500;

function test(array) {
    return array.join();
}
noInline(test);

var expected = test(array).length;
var total = 0;
for (var i = 0; i < 50; ++i)
    total += test(array).length;
if (total !== expected * 50)
    throw new Error("bad result: " + total);
