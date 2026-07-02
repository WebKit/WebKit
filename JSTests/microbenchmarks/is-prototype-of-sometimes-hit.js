(function() {
    class Foo { }
    var foo = new Foo();
    var fooProto = Foo.prototype;

    class Bar { }
    var bar = new Bar();

    function test(proto, o) {
        return proto.isPrototypeOf(o);
    }
    noInline(test);

    for (var i = 0; i < 5e6; ++i) {
        var o;
        if (i & 1)
            o = foo;
        else
            o = bar;
        if (test(fooProto, o) != !!(i & 1))
            throw "Error: bad result at i = " + i;
    }
})();
