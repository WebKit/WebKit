function foo(p) {
    var o = {f:42};
    if (p)
        throw o;
    return o;
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var o = foo(false);
    if (o.f != 42)
        throw "Error: bad result: " + o.f;
}

var didThrow = false;
try {
    foo(true);
} catch (e) {
    if (e.f != 42)
        throw "Error: bad result in catch: " + o.f;
    didThrow = true;
}
if (!didThrow)
    throw "Error: should have thrown but didn't.";
