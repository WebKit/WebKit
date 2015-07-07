(function(True) {
    var x = 0.5;
    var n = 1000000
    for (var i = 0; i < n; ++i)
        x += -True;
    if (x != 0.5 - n)
        throw "Error: bad result: " + x;
})(true);
