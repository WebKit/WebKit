function Foo() {
    this._f = 42;
}

Foo.prototype.__defineGetter__("f", function() { return this._f; });
Foo.prototype.__defineSetter__("f", function(value) { this._f = value; });

function foo(o, value) {
    for (var i = 0; i < 1000; ++i)
        o.f = value;
    return o.f;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(new Foo(), 42);
    if (result != 42)
        throw "Error: bad result: " + result;
}

