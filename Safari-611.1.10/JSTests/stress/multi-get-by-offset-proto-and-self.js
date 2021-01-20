function foo(o) {
    return o.f;
}

noInline(foo);

function Foo() { }
Foo.prototype.f = 42;

for (var i = 0; i < 100000; ++i) {
    if (i & 1) {
        var result = foo(new Foo());
        if (result != 42)
            throw "Error: bad result for new Foo(): " + result;
    } else {
        var result = foo({f:24});
        if (result != 24)
            throw "Error: bad result for {f:24}: " + result;
    }
}
