//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function(True) {
    var x = 42.5;
    var n = 1000000
    for (var i = 0; i < n; ++i)
        x /= True;
    if (x != 42.5)
        throw "Error: bad result: " + x;
})(true);
