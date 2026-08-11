//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function(True) {
    var x = 42;
    var n = 1000000
    for (var i = 0; i < n; ++i)
        x %= True;
    if (x != 0)
        throw "Error: bad result: " + x;
})(true);
