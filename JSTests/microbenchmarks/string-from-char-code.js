//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var result = (function() {
    var result;
    for (var i = 0; i < 10000000; ++i)
        result = String.fromCharCode(32);
    return result
})();

if (result != " ")
    throw "Error: bad result: " + result;

