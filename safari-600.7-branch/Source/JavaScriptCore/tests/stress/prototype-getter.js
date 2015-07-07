function Foo(g) {
    this.g_ = g;
}
Foo.prototype.__defineGetter__("f", function() { return this.g_ + 32; });
Foo.prototype.__defineGetter__("g", function() { return this.g_ + 33; });
Foo.prototype.__defineGetter__("h", function() { return this.g_ + 34; });
Foo.prototype.__defineGetter__("i", function() { return this.g_ + 35; });
Foo.prototype.__defineGetter__("j", function() { return this.g_ + 36; });
Foo.prototype.__defineGetter__("k", function() { return this.g_ + 37; });

function foo(o) {
    return o.f + o.k * 1000;
}

noInline(foo);

for (var i = 0; i < 100; ++i) {
    var result = foo(new Foo(5));
    if (result != (32 + 5) + (37 + 5) * 1000)
        throw "Error: bad result: " + result;
}
