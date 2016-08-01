function foo(x) {
    if (DFGTrue())
        x = "hello";
    return x + " world";
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({toString:function() { return "foo" }});
    if (result != "foo world" && result != "hello world")
        throw "Error: bad result: " + result;
}

