function bar() { }

function foo(alpha) {
    var x0 = 0;
    var x1 = 0;
    var x2 = 0;
    var x3 = 0;
    var x4 = 0;
    var x5 = 0;
    var x6 = 0;
    var x7 = 0;
    var x8 = 0;
    var x9 = 0;
    var x10 = 0;
    var x11 = 0;
    var x12 = 0;
    var x13 = 0;
    var x14 = 0;
    var x15 = 0;
    var x16 = 0;
    var x17 = 0;
    var x18 = 0;
    var x19 = 0;
    if (alpha) {
        bar(function () {
                return (x0 + x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10 +
                        x11 + x12 + x13 + x14 + x15 + x16 + x17 + x18 + x19);
                });
        return x17;
    }
    return x12;
}

noInline(bar);
noInline(foo);

for (var i = 0; i < 1000000; i++) {
    var result = foo(!(i % 1000));
    if (result !== 0)
        throw "Error: expected undefined, got " + result;
}
