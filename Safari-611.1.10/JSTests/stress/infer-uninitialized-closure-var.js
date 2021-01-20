function foo(p) {
    var x;
    
    noInline(f);
    
    if (p) {
        var f = function() { return x; }
        
        foo(false);
        
        for (var i = 0; i < 10000; ++i) {
            var result = f();
            if (result !== void 0)
                throw "Error: bad result (1): " + result;
        }
        
        x = 43;
        
        var result = f();
        if (result != 43)
            throw "Error: bad result (2): " + result;
    } else
        x = 42;
}

foo(true);
