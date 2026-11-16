function* items(n) {
    for (var i = 0; i < n; ++i)
        yield i;
}

function test() {
    return Array.from(items(8));
}
noInline(test);

var total = 0;
for (var i = 0; i < 5e5; ++i)
    total += test().length;
if (total !== 8 * 5e5)
    throw new Error("bad total: " + total);
