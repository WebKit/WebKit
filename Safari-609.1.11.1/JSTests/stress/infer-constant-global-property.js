function foo(p) {
    if (p)
        Math = {sin: function() { return 42; }, PI: 43, abs: Math.abs};
}

noInline(foo);

(function() {
    var n = 100000;
    var m = 100;
    var result = 0;
    for (var i = 0; i < n; ++i) {
        foo(i == n - m);
        result += Math.sin(Math.PI);
    }
    if (Math.abs(result - m * 42) > 1e-8)
        throw "Error: bad result: " + result;
})();
