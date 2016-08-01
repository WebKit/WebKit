function foo(a) {
    return a.f + 2000000000;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo({f:1});
    if (result != 2000000001)
        throw "Error: bad result: " + result;
}

var result = foo({f:2000000000});
if (result != 4000000000)
    throw "Error: bad result: " + result;
