//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    function Foo() { }
    Foo.prototype.f = 42;
    function Bar() { }
    Bar.prototype = new Foo();

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

    var o = new Bar();
    var p = new Bar();
    p.g = 43;
    
    foo(o, p);
})();
