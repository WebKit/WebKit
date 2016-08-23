(function() {
    var args = (function() {
        return arguments;
    })(1, 2, 3, 4, 5);
    
    var array = [args, [1, 2, 3]];
    
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += array[i % array.length].length;
    
    if (result != 4000000)
        throw "Error: bad result: " + result;
})();
