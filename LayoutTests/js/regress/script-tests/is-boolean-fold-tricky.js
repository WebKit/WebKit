var object = {};
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        var value1 = (i & 1) ? true : false;
        var value2 = (i & 1) ? "hello" : object;
        result += (typeof value1 == "boolean");
        result += (typeof value2 == "boolean") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
