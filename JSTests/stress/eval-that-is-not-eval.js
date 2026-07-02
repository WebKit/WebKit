function foo(a) {
    return eval(a);
}

noInline(foo);

eval = function(a) { return a + 1; }

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo(42);
    if (result != 43)
        throw "Error: bad result: " + result;
}

