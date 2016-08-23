(function() {
    var a = Symbol("Cocoa");
    var l = Symbol();
    var b = Symbol();
    var c = Symbol();

    var o = {[a]:1};
    var p = {[a]:2, [l]:13};
    var q = {[a]:3, [b]:3};
    var r = {[a]:4, [c]:5};
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        result ^= o[a];
        var tmp = o;
        o = p;
        p = q;
        q = r;
        r = tmp;
    }
    if (result != 0)
        throw "Error: bad result: " + result;
})();
