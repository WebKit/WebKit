(function(o, p) {
    var result = 0;
    var n = 1000000;
    for (var i = 0 ; i < n; ++i) {
        var a = o.f;
        var b = o.f;
        var c = o.f;
        var d = o.f;
        if (d) {
            var e = o.f;
            var f = o.f;
            var g = o.f;
            var h = o.f;
            if (h) {
                var j = o.f;
                var k = o.f;
                var l = o.f;
                var m = o.f;
                if (m) {
                    var q = o.f;
                    var r = o.f;
                    var s = o.f;
                    var t = o.f;
                    if (t)
                        result += r;
                }
            }
        }
        var tmp = o;
        o = p;
        p = tmp;
    }
    if (result != (n / 2) * o.f + (n / 2) * p.f)
        throw "Error: bad result: " + result;
})({f:42, g:0}, {g:0, f:43});
