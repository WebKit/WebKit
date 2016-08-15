function foo() {
    return /(f)(o)(o)/.test("foo");
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo();
    if (result != true)
        throw "Error: bad result: " + result;
}
