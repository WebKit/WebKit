function foo(a, b, c) {
    return a.f + b.f + c.f;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:2000000000}, {f:2000000000}, {f:0.5});
    if (result != 4000000000.5)
        throw "Error: bad result: " + result;
}

var result = foo({f:2000000000}, {f:2000000000}, {f:"42"});
if (result != "400000000042")
    throw "Error: bad result: " + result;

