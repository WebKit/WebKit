//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
(function() {
    var f = Symbol(), g = Symbol();
    var o;
    var n = 1000000;
    for (var i = 0; i < n; ++i) {
        if (i & 1)
            o = {[f]: 1};
        else
            o = {[f]: 1, [g]: 2};
        o[g] = i;
    }
    if (o[g] != n - 1)
        throw "Error: bad value of o.g: " + o[g];
})();
