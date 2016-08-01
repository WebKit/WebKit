function foo(o) {
    return typeof o.f === "symbol";
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
    if (result !== true)
        throw "Error: bad result: " + result;
}

bar(o, "hello");
var result = foo(o);
result = foo(o);
if (result !== false)
    throw "Error: bad result at end (false): " + result;
