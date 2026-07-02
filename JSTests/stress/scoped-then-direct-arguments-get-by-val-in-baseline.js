function direct() {
    return arguments;
}

function scoped(a) {
    if (!effectful42())
        return function() { return a; }
    return arguments;
}

function foo(a) {
    try {
        return a[0];
    } catch (e) {
        return -23;
    }
}

for (var i = 0; i < 100; ++i) {
    var result = foo(scoped(42));
    if (result != 42)
        throw "Error: bad result: " + result;
}

for (var i = 0; i < 100; ++i) {
    var result = foo(direct(42));
    if (result != 42)
        throw "Error: bad result: " + result;
}

