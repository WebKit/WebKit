var value1 = true;
var value2 = "hello";
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        result += (typeof value1 == "boolean");
        result += (typeof value2 == "boolean") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
