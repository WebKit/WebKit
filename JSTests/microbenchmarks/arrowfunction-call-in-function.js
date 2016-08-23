function bar(a, b) {
    return ((_a, _b) => _a + _b)(a, b);
}

noInline(bar);

for (let i = 0; i < 1000000; ++i) {
    let result = bar(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}
