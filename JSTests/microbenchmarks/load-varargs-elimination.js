function foo() {
    var result = 0;
    for (var i = 0; i < arguments.length; ++i)
        result += arguments[i];
    return result;
}

function bar() {
    return foo.apply(this, arguments);
}

function baz() {
    return bar(1, 2, 3, 4);
}

noInline(baz);

for (var i = 0; i < 1000000; ++i) {
    var result = baz();
    if (result != 10)
        throw "Error: bad result: " + result;
}

