(function() {
    var a = 'a', l = 'l';
    var o = {[a]:1};
    var p = {[a]:2, [l]:13};
    var result = 0;
    for (var i = 0; i < 1000000; ++i) {
        result ^= o[a];
        var tmp = o;
        o = p;
        p = tmp;
    }
    if (result != 0)
        throw "Error: bad result: " + result;
})();
