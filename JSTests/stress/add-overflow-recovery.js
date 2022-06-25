function foo(a, b) {
    return a.f + b.f;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:1}, {f:2});
    if (result != 3)
        throw "Error: bad result: " + result;
}

var result = foo({f:2000000000}, {f:2000000000});
if (result != 4000000000)
    throw "Error: bad result: " + result;
