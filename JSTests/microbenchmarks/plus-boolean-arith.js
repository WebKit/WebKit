//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function(True) {
    var x = 0;
    var n = 1000000
    for (var i = 0; i < n; ++i) {
        var y = True;
        y++;
        x += y;
    }
    if (x != n * 2)
        throw "Error: bad result: " + x;
})(true);
