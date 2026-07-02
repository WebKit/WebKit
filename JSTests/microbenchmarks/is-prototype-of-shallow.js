(function() {
    class Foo { }
    var foo = new Foo();
    var proto = Foo.prototype;

    function test(proto, foo) {
        return proto.isPrototypeOf(foo);
    }
    noInline(test);

    for (var i = 0; i < 5e6; ++i) {
        if (!test(proto, foo))
            throw "Error: bad result";
    }
})();
