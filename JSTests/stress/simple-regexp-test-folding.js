function foo() {
    return /(f)(o)(o)/.test("foo");
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo();
    if (result != true)
        throw "Error: bad result: " + result;
}
