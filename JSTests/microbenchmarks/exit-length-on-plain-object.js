function foo(o) {
    var result;
    for (var i = 0; i < 2; ++i) {
        var subResult = o.length;
        if (i == 0) {
            result = subResult;
            if (subResult !== void 0)
                break;
        }
        o = [1];
    }
    if (result === void 0) {
        for (var i = 0; i < 10000; ++i) { }
    }
    return result;
}

noInline(foo);

for (var j = 0; j < 10; ++j) {
    for (var i = 0; i < 10000; ++i) {
        var a = [1];
        a.length = 99999999;
        a.f = 42;
        foo(a);
    }
    
    var result = foo({});
    if (result !== void 0)
        throw "Error: bad result: " + result;
}
