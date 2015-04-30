var object = {};
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        var value1 = (i & 1) ? "hello" : "world";
        var value2 = (i & 1) ? 42 : object;
        result += (typeof value1 == "string");
        result += (typeof value2 == "string") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
