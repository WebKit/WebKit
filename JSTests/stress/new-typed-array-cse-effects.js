function foo(o, v)
{
    var x = o.f;
    new Int8Array(v);
    return x + o.f;
}

noInline(foo);

// Break some watchpoints.
var o = {f:42};
o.f = 43;

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:42}, [1, 2, 3]);
    if (result != 84)
        throw "Error: bad result: " + result;
}

var globalO = {f:42};
var weirdValue = {};
weirdValue.__defineGetter__("length", function() {
    globalO.f = 43;
    return 11;
});
var result = foo(globalO, weirdValue);
if (result != 85)
    throw "Error: bad result: " + result;
