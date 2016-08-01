function foo(a) {
    return a.f + 1000;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:1});
    if (result != 1001)
        throw "Error: bad result: " + result;
}

var result = foo({f:2147483148});
if (result != 2147484148)
    throw "Error: bad result: " + result;
