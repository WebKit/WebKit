(function() {
    function foo(x) { return 1; }
    function bar(x) { return x; }
    function baz(x) { return x + 1; }
    
    var n = 1000000;
    
    var result = (function(o) {
        var f = foo;
        var g = bar;
        var h = baz;
        
        var result = 0;
        for (var i = 0; i < n; ++i) {
            if (i == n - 1)
                f = h;
            result += f(o.f);
            
            var tmp = f;
            f = g;
            g = tmp;
        }
        
        return result;
    })({f:42});
    
    if (result != ((n / 2 - 1) * 42) + (n / 2 * 1) + (42 + 1))
        throw "Error: bad result: " + result;
})();
