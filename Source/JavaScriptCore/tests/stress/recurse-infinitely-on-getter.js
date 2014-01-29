for (var i = 0; i < 100; ++i) {
    var o = {};
    o.__defineGetter__("f", function() {
        return o.f;
    });
    var didThrow;
    var result;
    result = "not set";
    try {
        result = o.f;
    } catch (e) {
        didThrow = e;
    }
    if (result != "not set")
        throw "Did set result: " + result;
    if (!didThrow || didThrow.toString().indexOf("RangeError") != 0)
        throw "Bad exception: " + didThrow;
}

