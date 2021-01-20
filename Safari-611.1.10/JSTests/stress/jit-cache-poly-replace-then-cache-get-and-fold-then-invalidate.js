var o = {f:42};

function foo(p, o, v) {
    if (p)
        o.f = v;
}

function bar() {
    return o.f;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 10; ++i)
    foo(false);

for (var i = 0; i < 10; ++i)
    foo(true, {}, 42);

for (var i = 0; i < 10; ++i)
    foo(true, o, 42);

for (var i = 0; i < 100000; ++i) {
    var result = bar();
    if (result != 42)
        throw "Error: bad result: " + result;
}

foo(true, o, 53);
var result = bar();
if (result != 53)
    throw "Error: bad result at end: " + result;
