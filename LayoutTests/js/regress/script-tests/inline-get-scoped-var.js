function foo(x) {
    function bar(y) {
        return x + y;
    }
    
    var result = 0;
    for (var i = 0; i < 2000000; ++i)
        result = bar(1);
    
    return result;
}

var result = foo(5);
if (result != 6)
    throw "Bad result: " + result;
