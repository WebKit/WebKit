(function() {
    class Foo { }
    var foo = new Foo();
    
    class Bar { }
    var bar = new Bar();
    
    for (var i = 0; i < 5000000; ++i) {
        var o;
        if (i & 1)
            o = foo;
        else
            o = bar;
        if ((o instanceof Foo) != !!(i & 1))
            throw "Error: bad result at i = " + i;
    }
})();
