(function(){
    var a = 42;
    var b = 23;
    var c = 84;
    var d = 13;
    var e = 90;
    var f = 34;
    var g = 52;
    
    function foo() {
        return a + b + c + d + e + f + g;
    }
    
    noInline(foo);
    
    for (var i = 0; i < 500000; ++i) {
        var result = foo();
        if (result != 42 + 23 + 84 + 13 + 90 + 34 + 52)
            throw "Error: bad result: " + result;
    }
    
    a = 1;
    b = 2;
    c = 3;
    d = 4;
    e = 5;
    f = 6;
    g = 7;
    
    var result = foo();
    if (result != 1 + 2 + 3 + 4 + 5 + 6 + 7)
        throw "Error: bad result: " + result;
})();

