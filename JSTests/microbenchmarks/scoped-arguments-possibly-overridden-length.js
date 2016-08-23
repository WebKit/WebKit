(function() {
    var args1 = (function(a) {
        (function() {
            a++;
        })();
        return arguments;
    })(1, 2, 3);
    
    if (args1[0] != 2)
        throw "Error: bad args1: " + args1;
    
    var args2 = (function(a) {
        (function() {
            a++;
        })();
        var result = arguments;
        result.length = 6;
        return result;
    })(1, 2, 3, 4, 5);
    
    if (args2[0] != 2)
        throw "Error: bad args2: " + args2;
    
    var array = [args1, args2];
    
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += array[i % array.length].length;
    
    if (result != 4500000)
        throw "Error: bad result: " + result;
})();
