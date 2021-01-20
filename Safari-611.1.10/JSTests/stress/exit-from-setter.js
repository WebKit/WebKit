(function() {
    var o = {_f:42};
    o.__defineSetter__("f", function(value) { this._f = value * 100; });
    var n = 50000;
    function foo(o_, v_) {
        var o = o_.f;
        var v = v_.f;
        o.f = v;
        o.f = v + 1;
    }
    noInline(foo);
    for (var i = 0; i < n; ++i) {
        foo({f:o}, {f:11});
    }
    if (o._f != (11 + 1) * 100)
        throw "Error: bad o._f: " + o._f;
    for (var i = 0; i < n; ++i) {
        foo({f:o}, {f:1000000000});
    }
    if (o._f != 100 * (1000000000 + 1))
        throw "Error: bad o._f (2): " + o._f;
})();

