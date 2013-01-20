function foo(a,b) {
    var c = (a + b) / 2;
    var d = a - b;
    for (var i = 0; i < 100000; ++i) {
        c *= d;
        c += d;
        c /= d;
    }
    return c - d;
}

var result = 0;
for (var i = 0; i < 100; ++i) {
    result += foo(5,6);
}

if (result != 10000650) {
    print("Bad result: " + result);
    throw "Bad result: " + result;
}


