function Foo() {
}
Foo.prototype.f = 42;
Foo.prototype.g = 43;
Foo.prototype.h = 44;
Foo.prototype.i = 45;
Foo.prototype.j = 46;
Foo.prototype.k = 47;

function foo(o) {
    return o.f + o.k;
}

noInline(foo);

for (var i = 0; i < 100; ++i) {
    var result = foo(new Foo());
    if (result != 89)
        throw "Error: bad result for Foo: " + result;
}
