function foo(o) {
    return !!o.f;
}

function bar(o, v) {
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {};
for (var i = 0; i < 5; ++i)
    bar(o, true);

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result !== true)
        throw "Error: bad result: " + result;
}

bar(o, "hello");
var result = foo(o);
if (result !== true)
    throw "Error: bad result at end (true): " + result;
bar(o, "");
result = foo(o);
if (result !== false)
    throw "Error: bad result at end (false): " + result;
