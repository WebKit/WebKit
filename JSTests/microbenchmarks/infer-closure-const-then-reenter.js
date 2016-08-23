function foo(a_, b_, c_, d_, e_, f_, g_) {
    var a = a_;
    var b = b_;
    var c = c_;
    var d = d_;
    var e = e_;
    var f = f_;
    var g = g_;
    return {
        foo: function() {
            return a + b + c + d + e + f + g;
        }
    };
}

var thingy = foo(42, 23, 84, 13, 90, 34, 52);
for (var i = 0; i < 10000000; ++i) {
    var result = thingy.foo();
    if (result != 42 + 23 + 84 + 13 + 90 + 34 + 52)
        throw "Error: bad result: " + result;
}

var result = foo(1, 2, 3, 4, 5, 6, 7).foo();
if (result != 1 + 2 + 3 + 4 + 5 + 6 + 7)
    throw "Error: bad result: " + result;
