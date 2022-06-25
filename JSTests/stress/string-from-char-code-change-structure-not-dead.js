var globalString;

function foo(o, v)
{
    var x = o.f;
    globalString = String.fromCharCode(v);
    return x + o.f;
}

// Break some watchpoints.
var o = {f:24};
o.g = 43;

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:42}, {valueOf: function() { return 32; }});
    if (result != 84)
        throw "Error: bad result: " + result;
}

var globalO = {f:42};
var weirdValue = {valueOf: function() {
    delete globalO.f;
    globalO.__defineGetter__("f", function() { return 75; });
    return 33;
}};
var result = foo(globalO, weirdValue);
if (result != 42 + 75)
    throw "Error: bad result at end: " + result;

