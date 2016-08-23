(function() {
    var args1 = (function() {
        return arguments;
    })(1, 2, 3);
    
    var args2 = (function() {
        var result = arguments;
        result.length = 6;
        return result;
    })(1, 2, 3, 4, 5);
    
    var array = [args1, args2];
    
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += array[i % array.length].length;
    
    if (result != 4500000)
        throw "Error: bad result: " + result;
})();
