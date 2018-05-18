(function() {
    class Foo { }
    var foo = new Foo();
    
    for (var i = 0; i < 5000000; ++i) {
        if (!(foo instanceof Foo))
            throw "Error: bad result";
    }
})();
