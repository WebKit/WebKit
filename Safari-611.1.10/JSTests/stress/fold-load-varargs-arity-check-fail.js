function foo(a, b, c, d) {
    return [a, b, c, d];
}

function bar() {
    return foo.apply(this, arguments);
}

function baz() {
    return bar(42);
}

noInline(baz);

for (var i = 0; i < 10000; ++i) {
    var result = baz();
    if (!(result instanceof Array))
        throw "Error: result is not an array.";
    if (result.length != 4)
        throw "Error: result doesn't have length 4.";
    if (result[0] != 42)
        throw "Error: first element is not 42: " + result[0];
    for (var j = 1; j < 4; ++j) {
        if (result[j] !== void 0)
            throw "Error: element " + j + " is not undefined: " + result[j];
    }
}


