function foo(x) {
    return x == x;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo(0/0);
    if (result !== false)
        throw "Error: bad result: " + result;
}

