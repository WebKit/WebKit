var value1 = 42;
var value2 = "hello";
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        result += (typeof value1 == "number");
        result += (typeof value2 == "number") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
