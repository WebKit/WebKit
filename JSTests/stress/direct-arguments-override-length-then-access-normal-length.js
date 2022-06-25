(function() {
    var args = (function() {
        var result = arguments;
        result.length = 6;
        return result;
    })(1, 2, 3, 4, 5);
    
    var array = [args, [1, 2, 3]];
    
    function foo(thing) {
        return thing.length;
    }
    noInline(foo);
    
    var result = 0;
    for (var i = 0; i < 10000; ++i)
        result += foo(array[i % array.length]);
    
    if (result != 45000)
        throw "Error: bad result: " + result;
    
    var result = foo((function() { return arguments; })(1, 2, 3, 4));
    if (result != 4)
        throw "Error: bad result: " + result;
})();
