function foo(a, b) {
    return a < b;
}

noInline(foo);

var data = [
    [5, 6.5, true],
    ["foo", "bar", false],
    [true, false, false],
    [false, true, true],
    ["42", 42, false],
    [1.2, 1.2, false],
    ["-1", 1, true],
    [-1, "1", true]
];

for (var i = 0; i < 100000; ++i) {
    var test = data[i % data.length];
    var result = foo(test[0], test[1]);
    if (result != test[2])
        throw "Error: bad result for " + test + ": " + result;
}
