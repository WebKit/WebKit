function foo() {
    return isInt32(arguments);
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo();
    if (result !== false)
        throw "Error: bad result: " + result;
}

