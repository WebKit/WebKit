function foo(a, b) {
    var result = "";
    for (var i = 0; i < 100000; ++i) {
        result += a + b;
        result += a + b;
        result += a + b;
    }
    return result;
}

var result = foo("foo", "bar");
if (result.length != 1800000)
    throw "Bad result: " + result.length;
