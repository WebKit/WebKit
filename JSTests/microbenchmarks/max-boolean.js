(function(True) {
    var x = 0;
    var n = 1000000
    for (var i = 0; i < n; ++i)
        x += Math.max(True, 0);
    if (x != n)
        throw "Error: bad result: " + x;
})(true);
