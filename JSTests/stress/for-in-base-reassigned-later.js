function foo(o_) {
    var o = o_;
    var result = 0;
    for (var s in o) {
        result += o[s];
        if (result >= 3)
            o = {0:1, 1:2, a:3, b:4};
    }
    return result;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({0:0, 1:1, a:2, b:3});
    if (result != 7)
        throw "Error: bad result: " + result;
}
