function foo() {
    var p = {};
    var o = {};
    o.__proto__ = p;
    var result;
    for (var i = 0; i < 100; ++i) {
        result = o.f;
        if (i == 50)
            p.f = 42;
    }
    return result;
}

var result = foo();
if (result != 42)
    throw "Error: bad result: " + result;

