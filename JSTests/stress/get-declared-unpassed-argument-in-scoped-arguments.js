function foo(a) {
    if (!effectful42()) {
        (function() { a = 43; })();
        return arguments;
    }
    return a;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo();
    if (result !== void 0)
        throw "Error: bad result: " + result;
}
