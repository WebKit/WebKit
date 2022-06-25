var o = {f:42};

function foo(v) {
    o.f = v;
}

function bar() {
    return o.f;
}

noInline(foo);
noInline(bar);

foo(42);
foo(42);

for (var i = 0; i < 100000; ++i) {
    var result = bar();
    if (result != 42)
        throw "Error: bad result: " + result;
}

foo(53);
var result = bar();
if (result != 53)
    throw "Error: bad result at end: " + result;
