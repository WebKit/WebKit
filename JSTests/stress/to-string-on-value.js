function foo(o) {
    return String(o);
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(42);
    if (typeof result != "string") {
        describe(result);
        throw "Error: result isn't a string";
    }
    if (result != "42")
        throw "Error: bad result: " + result;
}
