function foo(o) {
    "use strict";
    o.f = 42;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var o = {};
    foo(o);
    if (o.f != 42)
        throw "Error: bad result: " + o.f;
    o = {f:23};
    foo(o);
    if (o.f != 42)
        throw "Error: bad result: " + o.f;
    o = {g:12};
    foo(o);
    if (o.f != 42)
        throw "Error: bad result: " + o.f;
}

var didThrow;
try {
    var o = {};
    Object.freeze(o);
    foo(o);
} catch (e) {
    didThrow = e;
}

if (!didThrow || didThrow.toString().indexOf("TypeError:") != 0)
    throw "Error: didn't throw or threw wrong exception: " + didThrow;
