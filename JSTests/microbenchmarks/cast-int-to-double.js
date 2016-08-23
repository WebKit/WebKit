function foo(o, a, b) {
    var x = o.f;
    return (x + a) - (x * 0.5) + (x + b) - (x * 0.1) + (a + 1) - (b - 1) + (x + 1) - (x - 1);
}

var o = {f:5};
var a = 0.2;
var b = 0.4;

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(o, a, b);

if (result != 11400000.00021128) {
    print(result);
    throw "Bad result";
}
