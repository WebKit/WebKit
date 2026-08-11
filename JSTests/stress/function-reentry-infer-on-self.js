function thingy(f) {
    f();
}
noInline(thingy);

function foo(a) {
    var x;
    if (a)
        x = a;
    thingy(function() { return x; });
    var result = 0;
    for (var i = 0; i < testLoopCount; ++i)
        result += x;
    return result;
}

noInline(foo);

for (var i = 0; i < 10; ++i) {
    var result = foo(42);
    if (result != 42 * testLoopCount)
        throw "Error: bad first result: " + result;
}

var result = foo(0);
if ("" + result != "NaN")
    throw "Error: bad result at end: " + result;

