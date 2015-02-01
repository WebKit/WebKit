description(
"Regression test for https://webkit.org/b/141111. This test should run without crashing."
);

function MyObject(v) {
    this.v = v;
}

function foo(o, a, b, c) {
    // Don't do anything real but have some control flow. This causes the PutLocals for a,
    // b, and c to survive into SSA form. But we don't have any effects, so sinking will be
    // successful.
    if (o.v)
        return o;
    else
        return z;
}

function bar(o, y) {
    var a = y;
    var b = y + 1;
    var c = y + 2;
    var d = y + 3;
    var e = y + 4;
    var f = y + 5;
    var g = y + 6;
    var h = y + 7;
    var i = y + 8;
    var j = y + 9;
    var k = y + 10;
    var result = function(p, q) {
        var x = new MyObject(a + b + c + d + e + f + g + h + i + j + k);
        if (q) {
            // Make it appear that it's possible to clobber those closure variables, so that we
            // load from them again down below.
            a = b = c = d = e = f = g = h = i = j = k = 42;
        }
        if (p)
            x = foo(o, 1, 2, 3)
        else
            x = five;
        return x.v + a + b + c + d + e + f + g + h + i + j + k;
    };
    noInline(result);
    return result;
}

var o = new MyObject(42);
var z = new MyObject(0);
var five = new MyObject(5);

for (var i = 0; i < 100000; ++i) {
    var result = bar(o, i)(true, false);
    if (result != 42 + 11 * i + 55)
        throw "Error: bad result: " + result;
}

