var f;

function foo(s) {
    var x = 1;
    f = function() { return x; };
    x = 2;
    new Array(s);
    x = 3;
}

noInline(foo);

for (var i = 0; i < 10000; ++i)
    foo(1);

var didThrow = false;
try {
    foo(-1);
} catch (e) {
    didThrow = e;
}

if (("" + didThrow).indexOf("RangeError") != 0)
    throw "Error: did not throw right exception: " + didThrow;

var result = f();
if (result != 2)
    throw "Error: bad result from f(): " + result;
