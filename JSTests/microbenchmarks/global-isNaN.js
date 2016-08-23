(function() {
    var result = 0;
    var values = [0, -1, 123.45, Infinity, NaN];
    for (var i = 0; i < 1000000; ++i) {
        for (var j = 0; j < values.length; ++j) {
            if (isNaN(values[j]))
                result++;
        }
    }
    if (result !== 1000000)
        throw "Error: bad result: " + result;
})();
