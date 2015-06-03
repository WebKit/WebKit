function Foo() {
    this._f = 42;
}

Foo.prototype.__defineGetter__("f", function() { return this._f; });

function foo(o) {
    var result = 0;
    for (var i = 0; i < 1000; ++i)
        result += o.f;
    return result;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(new Foo());
    if (result != 1000 * 42)
        throw "Error: bad result: " + result;
}

