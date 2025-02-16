function foo(a, b) {
    return [a, b];
}

function bar() {
    return foo.apply(null, arguments);
}

noInline(bar);

for (var i = 0; i < testLoopCount; ++i) {
    var result = bar(1);
    if ("" + result != "1,")
        throw "Error: bad result: " + result;
}

