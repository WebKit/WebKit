function foo(value) {
    return !!value;
}

noInline(foo);

var tests = [
    [0, false],
    [1, true],
    [0/0, false],
    [0/-1, false],
    [0.0, false],
    ["", false],
    ["f", true],
    ["hello", true],
    [{}, true],
    [[], true],
    [null, false],
    [void 0, false],
    [false, false],
    [true, true]
];

for (var i = 0; i < 10000; ++i) {
    for (var j = 0; j < tests.length; ++j) {
        var input = tests[j][0];
        var expected = tests[j][1];
        var result = foo(input);
        if (result !== expected)
            throw "Error: bad result for " + input + ": " + result;
    }
}
