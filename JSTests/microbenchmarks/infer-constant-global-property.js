(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        result += Math.sin(Math.PI);
    }
    if (Math.abs(result) > 1e-8)
        throw "Error: bad result: " + result;
})();
