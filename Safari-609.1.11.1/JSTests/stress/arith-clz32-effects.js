function foo(o, v)
{
    var result = o.f;
    Math.clz32(v);
    return result + o.f;
}

noInline(foo);

var o = {f: 42};
o.g = 43; // Bust the transition watchpoint of {f}.

for (var i = 0; i < 10000; ++i) {
    var result = foo({f: 42}, "42");
    if (result != 84)
        throw "Error: bad result in loop: " + result;
}

var o = {f: 43};
var result = foo(o, {
    valueOf: function()
    {
        delete o.f;
        o.__defineGetter__("f", function() { return 44; });
    }
});

if (result != 87)
    throw "Error: bad result at end: " + result;

