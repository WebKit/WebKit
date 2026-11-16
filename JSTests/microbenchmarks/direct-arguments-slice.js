var slice = Array.prototype.slice;
function test(a, b, c, d, e, f, g, h, i, j, k, l)
{
    return slice.call(arguments);
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    test(1, "hello", null, undefined, 42, Array, Symbol.iterator, false, 42.195, 0, -200, -44.2);
}
