//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function(True) {
    var x = 0;
    var n = 1000000
    for (var i = 0; i < n; ++i)
        x = Math.sin(True);
    if (x != Math.sin(1))
        throw "Error: bad result: " + x;
})(true);
