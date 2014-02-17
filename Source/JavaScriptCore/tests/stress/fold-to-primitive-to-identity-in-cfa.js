function foo(x, p) {
    if (DFGTrue())
        x = p ? "hello" : "bar";
    return x + " world";
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({toString:function() { return "foo" }}, i & 1);
    if (result != "foo world" && result != "hello world" && result != "bar world")
        throw "Error: bad result: " + result;
}

