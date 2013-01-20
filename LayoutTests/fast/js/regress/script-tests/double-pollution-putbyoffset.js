function foo(a, b) {
    var c = (a + b) / 2;
    var d = a - b;
    for (var i = 0; i < 2; ++i) {
        c *= d;
        c += d;
        c /= d;
    }
    return {a:c - d, b:d};
}

var result = 0;
var array = [7, 8];
for (var i = 0; i < 100000; ++i) {
    var thingy = foo(5, 6);
    result += thingy.a + array[2 + thingy.b];
}

if (result != 1650000) {
    print("Bac result: " + result);
    throw "Bad result: " + result;
}


