function foo() {
    return /(f)(o)(o)/.exec("bar");
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo();
    if (result !== null)
        throw "Error: bad result: " + result;
}
