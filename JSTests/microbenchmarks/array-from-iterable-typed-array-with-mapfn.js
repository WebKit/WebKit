var items = new Int32Array([0, 1, 2, 3, 4, 5, 6, 7]);
var mapFn = (x) => x;

function test() {
    return Array.from(items, mapFn);
}
noInline(test);

var total = 0;
for (var i = 0; i < 5e5; ++i)
    total += test().length;
if (total !== 8 * 5e5)
    throw new Error("bad total: " + total);
