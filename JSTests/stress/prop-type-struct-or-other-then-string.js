String.prototype.g = 44;

function foo(o) {
    var tmp = o.f;
    if (tmp)
        return tmp.g;
    return 42;
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
    if (result !== 42)
        throw "Error: bad result for o: " + result;
    result = foo(p);
    if (result !== 43)
        throw "Error: bad result for p: " + result;
}

bar(o, "hello");
var result = foo(o);
if (result !== 44)
    throw "Error: bad result at end: " + result;
