(function() {
    function foo(x) { return 1 + this.f; }
    function bar(x) { return x + this.f; }
    function baz(x) { return x + 1 + this.f; }
    
    var n = 1000000;
    
    var result = (function(o) {
        var f = {fun:foo, f:1};
        var g = {fun:bar, f:2};
        var h = {fun:baz, f:3};
        
        var result = 0;
        for (var i = 0; i < n; ++i) {
            if (i == n - 1)
                f = h;
            result += f.fun(o.f);
            
            var tmp = f;
            f = g;
            g = tmp;
        }
        
        return result;
    })({f:42});
    
    if (result != ((n / 2 - 1) * (42 + 2)) + (n / 2 * (1 + 1) + (42 + 1 + 3)))
        throw "Error: bad result: " + result;
})();
