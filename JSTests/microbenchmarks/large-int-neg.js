function foo(x, y) {
    var z = x;
    var w = 0;
    for (var i = 0; i < 3; ++i) {
        z += y;
        w = -z;
    }
    return w + 1;
}

eval("// Don't compile me");

var result = 0;

for (var i = 0; i < 1000000; ++i)
    result += foo(1000000000, 1000000000);

if (result != -3999999999000000)
    throw "Error: bad result: " + result;
