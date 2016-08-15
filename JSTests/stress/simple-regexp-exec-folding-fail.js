function foo() {
    return /(f)(o)(o)/.exec("bar");
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo();
    if (result !== null)
        throw "Error: bad result: " + result;
}
