var object = {};
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        var value1 = void 0;
        var value2 = (i & 1) ? "hello" : object;
        result += (typeof value1 == "undefined");
        result += (typeof value2 == "undefined") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
