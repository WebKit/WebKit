//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function(a, b) {
    var n = 200000;
    var result = 0;
    for (var i = 0; i < n; ++i) {
        result += a.localeCompare(b);
    }
    if (result != n)
        throw "Error: bad result: " + result;
})("yes", "no");

