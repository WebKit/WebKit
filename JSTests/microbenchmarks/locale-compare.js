(function(a, b) {
    var n = 200000;
    var result = 0;
    for (var i = 0; i < n; ++i) {
        result += a.localeCompare(b);
    }
    if (result != n)
        throw "Error: bad result: " + result;
})("yes", "no");

