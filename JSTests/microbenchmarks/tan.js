(function() {
    for (var i = 0; i < 3000000; ++i)
        x = Math.tan(i);

    if (x != 1.8222665884307354)
        throw "Error: bad result: " + x;
})();
