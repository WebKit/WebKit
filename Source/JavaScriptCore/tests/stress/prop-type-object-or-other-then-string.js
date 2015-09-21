String.prototype.g = 44;

function foo(o) {
    return o.f == {toString:function() { return "hello"; }};
}

function bar(o, v) {
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {};
var p = {};
for (var i = 0; i < 5; ++i)
    bar(o, null);
for (var i = 0; i < 5; ++i)
    bar(p, {g:43});

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result !== false)
        throw "Error: bad result for o: " + result;
    result = foo(p);
    if (result !== false)
        throw "Error: bad result for p: " + result;
}

bar(o, "hello");
var result = foo(o);
if (result !== true)
    throw "Error: bad result at end: " + result;
