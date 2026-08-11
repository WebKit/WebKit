var slice = Array.prototype.slice;
function test(a, b, c, d, e, f, g, h, i, j, k, l)
{
    // Reference a parameter to make arguments a ScopedArguments
    (function () { return a; })();
    return slice.call(arguments);
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    test(1, "hello", null, undefined, 42, Array, Symbol.iterator, false, 42.195, 0, -200, -44.2);
}
