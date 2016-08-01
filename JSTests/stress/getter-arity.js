var o = {};
o.__defineGetter__("f", function(a, b, c, d, e, f) { return 42; });
for (var i = 0; i < 10000; ++i) {
    var result = o.f;
    if (result != 42)
        throw "Error: bad result: " + result;
}
