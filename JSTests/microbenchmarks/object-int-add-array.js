(function() {
    var result = 0;
    var m = 100;
    var n = 100000;
    var array = [];
    for (var i = 0; i < 100; ++i)
        array.push(12);
    for (var i = 0; i < m; ++i) {
        var value = 0;
        var o = {valueOf: function() { return 42; }};
        value += o;
        var result = 0;
        for (var j = 0; j < n; ++j)
            result += array[value];
    }
    if (result != n * 12)
        throw "Error: bad result: " + result;
})();
