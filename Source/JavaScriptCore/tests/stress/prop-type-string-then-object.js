function foo(o) {
    return o.f + " world";
}

function bar(o, v) {
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {};
for (var i = 0; i < 5; ++i)
    bar(o, "hello");

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result != "hello world")
        throw "Error: bad result: " + result;
}

bar(o, {toString: function() { return "hello"; }});
var result = foo(o);
if (result != "hello world")
    throw "Error: bad result at end: " + result;
