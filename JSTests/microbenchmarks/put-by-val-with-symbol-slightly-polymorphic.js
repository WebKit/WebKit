(function() {
    var f = Symbol(), g = Symbol();
    var o = {[f]: 1, [g]: 2};
    var p = {[f]: 1};
    var n = 1000000;
    for (var i = 0; i < n; ++i) {
        o[f] = i;
        var tmp = o;
        o = p;
        p = tmp;
    }
    if (o[f] != n - 2)
        throw "Error: bad value of o.f: " + o[f];
    if (p[f] != n - 1)
        throw "Error: vad value of p.f: " + p[f];
})();
