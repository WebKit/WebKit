(function() {
    var args = (function(a) {
        (function() {
            a++;
        })();
        var result = arguments;
        result.length = 6;
        return result;
    })(1, 2, 3, 4, 5);
    
    if (args[0] != 2)
        throw "Error: bad args: " + args;
    
    var array = [args, [1, 2, 3]];
    
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += array[i % array.length].length;
    
    if (result != 4500000)
        throw "Error: bad result: " + result;
})();
