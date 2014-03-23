function foo(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z) {
    return a * 2 + b * 3 + c * 5 + d * 7 + e * 11 + f * 13 + g * 17 + h * 19 + i * 23 + j * 29 + k * 31 + l * 37 + m * 41 + n * 43 + o * 47 + p * 53 + q * 59 + r * 61 + s * 67 + t * 71 + u * 73 + v * 79 + w * 83 + x * 89 + y * 97 + z * 101;
}

function bar() {
    return foo.apply(this, arguments);
}

function baz(x) {
    return bar(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26) + x;
}

noInline(baz);

for (var i = 0; i < 10000; ++i) {
    var result = baz(0);
    if (result != 21586)
        throw "Error: bad result: " + result;
}

// Force recompilation.
for (var i = 0; i < 10000; ++i) {
    var result = baz(2147483646);
    if (result != 2147505232)
        throw "Error: bad result: " + result;
}
