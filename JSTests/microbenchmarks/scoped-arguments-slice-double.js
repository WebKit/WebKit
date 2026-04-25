var slice = Array.prototype.slice;
function test(a, b, c, d, e, f, g, h, i, j, k, l)
{
    // Reference a parameter to make arguments a ScopedArguments
    (function () { return a; })();
    return slice.call(arguments);
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    test(1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8, 9.9, 10.1, 11.2, 12.3);
}
