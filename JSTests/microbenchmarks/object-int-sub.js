(function() {
    var result = 0;
    var m = 1000;
    var n = 10000;
    for (var i = 0; i < m; ++i) {
        var o = {valueOf: function() { return 42; }};
        result -= o;
        for (var j = 0; j < n; ++j)
            result -= j;
    }
    if (result != -m * (42 + n * (n - 1) / 2))
        throw "Error: bad result: " + result;
})();
