function foo(x) {
    return (x + 0) + (x + 1) + (x + 2) + (x + 3) + (x + 4) + (x + 5) + (x + 6) + (x + 7) + (x + 8) + (x + 9) + (x + 10);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(i);
    if (result != i * 11 + 55)
        throw "Error: bad result for i = " + i + ": " + result;
}

for (var i = 2147483628; i <= 2147483647; i++) {
    var result = foo(i);
    if (result != i * 11 + 55)
        throw "Error: bad result for i = " + i + ": " + result;
}

