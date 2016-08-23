var value1 = "hello";
var value2 = 42;
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        result += (typeof value1 == "string");
        result += (typeof value2 == "string") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
