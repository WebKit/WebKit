(function() {
    var x = 42;
    var result = 0;
    var n = 100000;
    var m = 100;
    for (var i = 0; i < n; ++i) {
        result += x;
        (function() {
            with ({}) {
                if (i == n - m - 1)
                    x = 53;
            }
        })();
    }
    if (result != 42 * (n - m) + 53 * m)
        throw "Error: bad result: " + result;
})();
