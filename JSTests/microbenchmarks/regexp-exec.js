//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var result = 0;
    var n = 1000000;
    var re = /foo/;
    for (var i = 0; i < n; ++i)
        result += re.exec("foo").length;
    if (result != n)
        throw "Error: bad result: " + result;
})();
