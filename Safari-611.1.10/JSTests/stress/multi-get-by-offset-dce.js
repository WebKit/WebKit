function foo(o) {
    var tmp = o.f;
    return 42;
}

noInline(foo);

var array = [{f:1}, {g:1, f:2}];
for (var i = 0; i < 10000; ++i) {
    var result = foo(array[i % array.length]);
    if (result != 42)
        throw "Error: bad result in loop: " + result;
}

var o = {};
var didCallGetter = false;
o.__defineGetter__("f", function() { didCallGetter = true; return 73; });
var result = foo(o);
if (result != 42)
    throw "Error: bad result at end: " + result;
if (!didCallGetter)
    throw "Error: did not call getter at end.";
