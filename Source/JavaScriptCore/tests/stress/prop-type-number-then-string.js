function foo(o) {
    return o.f + 1;
}

function bar(o, v) {
    o.f = v;
}

noInline(foo);
noInline(bar);

var o = {};
for (var i = 0; i < 5; ++i)
    bar(o, 4.2);

for (var i = 0; i < 10000; ++i) {
    var result = foo(o);
    if (result !== 5.2)
        throw "Error: bad result: " + result;
}

bar(o, "hello");
var result = foo(o);
if (result !== "hello1")
    throw "Error: bad result at end: " + result;
