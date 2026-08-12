var seed = 42;
function next() {
    seed = (seed + 0x9e3779b9) | 0;
    var t = seed ^ (seed >>> 16);
    t = Math.imul(t, 0x21f0aaad);
    t = t ^ (t >>> 15);
    t = Math.imul(t, 0x735a2d97);
    return (t ^ (t >>> 15)) & 0x7fffffff;
}

var ids = [];
for (var i = 0; i < 100; ++i)
    ids.push(next() % 1000000);

function test(array) {
    return array.join(",");
}
noInline(test);

var total = 0;
for (var i = 0; i < 2e4; ++i)
    total += test(ids).length;
if (total !== test(ids).length * 2e4)
    throw new Error("bad result: " + total);
