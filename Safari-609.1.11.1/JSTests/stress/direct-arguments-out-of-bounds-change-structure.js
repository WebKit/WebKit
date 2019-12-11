function foo(o)
{
    var theO = o;
    var x = theO.f;
    arguments[42];
    return x + theO.f;
}

// Break some watchpoints.
var o = {f:24};
o.g = 43;

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:42});
    if (result != 84)
        throw "Error: bad result: " + result;
}

var globalO = {f:42};
Object.prototype.__defineGetter__(42, function() {
    delete globalO.f;
    globalO.__defineGetter__("f", function() { return 75; });
    return 33;
});
var result = foo(globalO);
if (result != 42 + 75)
    throw "Error: bad result at end: " + result;

