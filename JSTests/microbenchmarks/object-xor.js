(function() {
    var m = 1000;
    var n = 1000;
    var o = {valueOf: function() { return 42; }};
    var result = 0;
    for (var i = 0; i < m; ++i) {
        result += o ^ 2;
        for (var j = 0; j < n; ++j)
            result *= 1.1;
        for (var j = 0; j < n; ++j)
            result /= 2;
    }
    
    if (result != 9.220394130074086e-259)
        throw "Error: bad result: " + result;
})();

