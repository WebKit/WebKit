(function() {
    function Foo() { }
    Foo.prototype.f = 42;
    function Bar() { }
    Bar.prototype = new Foo();
    function Baz() { }
    Baz.prototype = new Foo();
    
    function foo(o, p) {
        var n = 1000000;
        var result = 0;
        for (var i = 0; i < n; ++i) {
            result += o.f;
            var tmp = o;
            o = p;
            p = tmp;
        }
        
        if (result != n * 42)
            throw "Error: bad result: " + result;
    }

    foo(new Bar(), new Baz());
})();
