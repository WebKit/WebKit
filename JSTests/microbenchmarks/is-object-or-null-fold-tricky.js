//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var object = {};
(function() {
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        var value1 = (i & 1) ? null : object;
        var value2 = (i & 1) ? 42 : "hello";
        result += (typeof value1 == "object");
        result += (typeof value2 == "object") << 1;
    }
    if (result != 1000000)
        throw "Error: bad result: " + result;
})();
