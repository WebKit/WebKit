// Regression test for https://bugs.webkit.org/show_bug.cgi?id=147165

function Foo() { }
Foo.prototype.f = 42;

function get(o, p) {
    if (p)
        return o.f;
    return 42;
}

for (var i = 0; i < 100000; ++i) {
    get({ f: 42 }, i % 2);
    get({ o: 10, f: 42 }, i % 2);
}

function foo() {
    var o = new Foo();
    return get(o, isFinalTier());
}
noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo();
    if (result !== 42)
        throw new Error("Result should be 42 but was " + result);
}
