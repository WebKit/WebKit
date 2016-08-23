var value1 = function() { };
var value2 = 42;
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        result += (typeof value1 == "function");
        result += (typeof value2 == "function") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
