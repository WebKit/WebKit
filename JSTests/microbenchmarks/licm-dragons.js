function foo(p, o) {
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        if (p)
            result += o.f;
        else
            result++;
    }
    return result;
}

noInline(foo);

var o = {f:42};
var p = {};

var result = 0;

for (var i = 0; i < 1000; ++i) {
    result += foo(true, o);
    result += foo(false, p);
}

for (var i = 0; i < 10000; ++i)
    result += foo(false, p);

for (var i = 0; i < 30000; ++i)
    result += foo(true, o);

if (result != 1313000000)
    throw "Error: bad result: " + result;
