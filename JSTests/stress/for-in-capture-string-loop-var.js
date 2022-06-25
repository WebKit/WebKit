(function() {
    // Capture the loop variable and modify it inside the loop.
    var foo = function() {
        var captured;
        var g = function() {
            captured = "foo";
        };
        var sum = 0;
        var o = {"foo": 1, "bar": 2};
        for (captured in o) {
            g();
            sum += o[captured];
        }
        return sum;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() != 2)
            throw new Error("bad result");
    }
    foo(null);
})();
