function foo(o) {
    return typeof o.f;
}

function bar(o, v) {
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {};
for (var i = 0; i < 5; ++i)
    bar(o, Symbol("Cocoa"));

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result != "symbol")
        throw "Error: bad result: " + result;
}

bar(o, {toString: function() { return "hello"; }});
var result = foo(o);
if (result != "object")
    throw "Error: bad result at end: " + result;
