function foo(x) {
    return "hello" + x + "!!";
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(" world");
    if (typeof result != "string") {
        describe(result);
        throw "Error: bad result type: " + result;
    }
    if (result != "hello world!!")
        throw "Error: bad result: " + result;
}

