function foo() {
    var o = {};
    o.a = 1;
    o.b = 2;
    o.c = 3;
    o.d = 4;
    o.e = 5;
    o.f = 6;
    o.g = 7;
    return o;
}

var result = 0;
for (var i = 0; i < 100000; ++i)
    result += foo().a;

if (result != 100000)
    throw "Error, bad result: " + result;

