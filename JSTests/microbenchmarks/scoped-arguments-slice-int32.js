var slice = Array.prototype.slice;
function test(a, b, c, d, e, f, g, h, i, j, k, l)
{
    // Reference a parameter to make arguments a ScopedArguments
    (function () { return a; })();
    return slice.call(arguments);
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    test(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
}
