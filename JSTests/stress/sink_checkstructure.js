function foo(p, q) {
    var o = {};
    if (p) o.f = 42;
    if (q) { o.f++; return o; }
}
noInline(foo);

var expected = foo(false, true).f;

for (var i = 0; i < 1000000; i++) {
    foo(true, true);
}

var result = foo(false, true).f;

if (!Object.is(result, expected))
    throw "Error: expected " + expected + "; FTL produced " + result;
