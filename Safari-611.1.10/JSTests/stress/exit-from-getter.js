(function() {
    var o = {_f:42};
    o.__defineGetter__("f", function() { return this._f * 100; });
    var result = 0;
    var n = 50000;
    function foo(o) {
        return o.f + 11;
    }
    noInline(foo);
    for (var i = 0; i < n; ++i) {
        result += foo(o);
    }
    if (result != n * (42 * 100 + 11))
        throw "Error: bad result: " + result;
    o._f = 1000000000;
    result = 0;
    for (var i = 0; i < n; ++i) {
        result += foo(o);
    }
    if (result != n * (1000000000 * 100 + 11))
        throw "Error: bad result (2): " + result;
})();

