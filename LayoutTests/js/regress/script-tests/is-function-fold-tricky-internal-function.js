var object = {};
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        var value1 = (i & 1) ? Map : Set;
        var value2 = (i & 1) ? object : "hello";
        result += (typeof value1 == "function");
        result += (typeof value2 == "function") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
