function foo() {
    return /(f)(o)(o)/.exec("foo");
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo();
    if (result.length != 4)
        throw "Error: bad result: " + result;
    if (result[0] != "foo")
        throw "Error: bad result: " + result;
    if (result[1] != "f")
        throw "Error: bad result: " + result;
    if (result[2] != "o")
        throw "Error: bad result: " + result;
    if (result[3] != "o")
        throw "Error: bad result: " + result;
}
