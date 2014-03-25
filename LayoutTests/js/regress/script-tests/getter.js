(function() {
    var o = {_f:42};
    o.__defineGetter__("f", function() { return this._f; });
    var result = 0;
    var n = 2000000;
    for (var i = 0; i < n; ++i)
        result += o.f;
    if (result != n * 42)
        throw "Error: bad result: " + result;
})();

