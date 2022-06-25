(function() {
    // Change string value of the loop variable in the loop.
    var foo = function() {
        var sum = 0;
        var a = [1, 2, 3];
        a.foo = 42;
        for (var i in a) {
            i = "foo";
            sum += a[i];
        }
        return sum;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() != 42 * 4)
            throw new Error("bad result");
    }
    foo(null);
})();
