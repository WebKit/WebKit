(function() {
    var o = {_f:42};
    o.__defineSetter__("f", function(value) { this._f = value; });
    var n = 2000000;
    for (var i = 0; i < n; ++i)
        o.f = i;
    if (o._f != n - 1)
        throw "Error: bad result: " + o._f;
})();

