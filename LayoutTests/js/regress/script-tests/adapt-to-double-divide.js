function foo(a,b) {
    var c = (a + b) / 2;
    var d = a - b;
    for (var i = 0; i < 50000; ++i) {
        c *= d;
        c += d;
        c /= d;
    }
    return c - d;
}

var result = 0;
for (var i = 0; i < 50; ++i) {
    result += foo(5,6);
}

if (result != 2500325)
    throw "Bad result: " + result;


