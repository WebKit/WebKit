function foo(a, b, array) {
    var c = (a + b) / 2;
    var d = a - b;
    for (var i = 0; i < 100000; ++i) {
        c *= d;
        c += d;
        c /= d;
        c += array[d + 2];
    }
    return c - d;
}

var result = 0;
var array = [7, 8];
for (var i = 0; i < 10; ++i) {
    result += foo(5, 6, array);
}

if (result != 9000065) {
    print("Bad result: " + result);
    throw "Bad result: " + result;
}

