function foo(p) {
    var f1 = function (x) { return x; };
    var f2 = function (x) { return x; };
    var f3 = function (x) { return x; };
    var f4 = function (x) { return x; };
    var f5 = function (x) { return x; };
    var f6 = function (x) { return x; };
    var f7 = function (x) { return x; };
    var f8 = function (x) { return x; };
    var f9 = function (x) { return x; };
    var f10 = function (x) { return x; };
    var f11 = function (x) { return x; };
    var f12 = function (x) { return x; };
    var f13 = function (x) { return x; };
    var f14 = function (x) { return x; };
    var f15 = function (x) { return x; };
    var f16 = function (x) { return x; };
    var f17 = function (x) { return x; };
    var f18 = function (x) { return x; };
    var f19 = function (x) { return x; };
    if (p)
        return f1(f2(f3(f4(f5(f6(f7(f8(f9(f10(f11(f12(f13(f14(f15(f16(f17(f18(f19(p)))))))))))))))))));
}
noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo(false);
    if (result)
        throw "Error: bad result: " + result;
}

var result = foo(true);
if (result !== true)
    throw "Error: bad result: " + result;

