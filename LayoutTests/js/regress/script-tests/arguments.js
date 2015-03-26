function foo() {
    return arguments[0];
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo(i);
    if (result != i)
        throw "Error: bad result: " + result;
}
