function foo() {
    arguments = {f:42};
    return arguments.f;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo();
    if (result != 42)
        throw "Error: bad result: " + result;
}

