function foo(a, b) {
    return a == b;
}

noInline(foo);

var data = [
    [5, 6.5, false],
    ["foo", "bar", false],
    [true, false, false],
    ["42", 42, true],
    [1.2, 1.2, true]
];

for (var i = 0; i < 100000; ++i) {
    var test = data[i % data.length];
    var result = foo(test[0], test[1]);
    if (result != test[2])
        throw "Error: bad result for " + test + ": " + result;
}
