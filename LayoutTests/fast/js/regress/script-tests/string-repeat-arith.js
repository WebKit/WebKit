function foo(a, b) {
    var c = a - b;
    var d = a * b;
    var e = a / b;
    var f = a | b;
    var g = a ^ b;
    var h = a & b;
    return c + d - e + f - g + h;
}

var result = 0;
for (var i = 0; i < 100000; ++i)
    result ^= foo("42", i);

if (result != 3472169)
    throw "Bad result: " + result;
