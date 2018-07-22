(function() {
    var globalO;
    
    function polyvariant()
    {
        return globalO.func();
    }
    
    class Foo {
        func()
        {
            return 42;
        }
    }
    
    var fooO = new Foo();
    
    function foo()
    {
        globalO = fooO;
        return polyvariant();
    }
    
    class Bar {
        func()
        {
            return foo();
        }
    }
    
    var barO = new Bar();
    
    function bar()
    {
        globalO = barO;
        return polyvariant();
    }
    
    var count = 1000000;
    var result = 0;
    for (var i = 0; i < count; ++i)
        result += bar();
    
    if (result != count * 42)
        throw "Error: bad result: " + result;
})();
