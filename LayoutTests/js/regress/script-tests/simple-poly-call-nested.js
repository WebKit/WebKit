(function() {
    function foo(x) { return 1; }
    function bar(x) { return x; }
    
    var n = 1000000;
    
    var result = (function() {
        var f = foo;
        var g = bar;
        
        var result = 0;
        for (var i = 0; i < n; ++i) {
            result += f(42);
            
            var tmp = f;
            f = g;
            g = tmp;
        }
        
        return result;
    })();
    
    if (result != n / 2 * 42 + n / 2 * 1)
        throw "Error: bad result: " + result;
})();
