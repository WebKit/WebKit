var af = (a, b) => a + b;

noInline(af);

function bar(a, b) {
    return af(a, b);
}

noInline(bar);

for (var i = 0; i < 1000000; ++i) {
    var result = bar(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}
