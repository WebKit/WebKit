var items = "abcdefgh";
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
