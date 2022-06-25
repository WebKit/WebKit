function foo(a) {
    var x = a + 1;
    return function(a) {
        return x + a;
    };
}

var f = foo(42);
noInline(f);

for (var i = 0; i < 10000; ++i) {
    var result = f(i);
    if (result != 42 + 1 + i)
        throw "Error: bad result: " + result;
}

var f = foo(43);
var result = f(1);
if (result != 43 + 1 + 1)
    throw "Error: bad result: " + result;
