function foo(o_) {
    var o = o_;
    var result = 0;
    for (var s in o) {
        result += o[s];
    }
    return result;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(createProxy({a:1, b:2, c:3, d:4}));
    if (result != 1 + 2 + 3 + 4)
        throw "Error: bad result: " + result;
}
