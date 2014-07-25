(function() {
    function Foo() { }
    Foo.prototype.f = 42;
    function Bar() { }
    Bar.prototype = new Foo();
    
    var o = new Bar();
    var p = new Bar();
    p.g = 43;
    
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
})();
