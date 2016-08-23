(function() {
    var m = 1000;
    var n = 1000;
    var o = {valueOf: function() { return 1; }};
    var result = 0;
    for (var i = 0; i < m; ++i) {
        result += 2 >> o;
        for (var j = 0; j < n; ++j)
            result *= 1.1;
        for (var j = 0; j < n; ++j)
            result /= 2;
    }
    
    if (result != 2.3050985325185195e-260)
        throw "Error: bad result: " + result;
})();

