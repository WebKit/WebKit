(function() {
    // Change integer value of the loop variable in the loop.
    var foo = function() {
        var a = [1, 2, 3];
        var sum = 0;
        for (var i in a) {
            i += 10;
            sum += i;
        }
        return sum;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        var result = foo();
        if (typeof result !== "string")
            throw new Error("result should have type string");
        if (result !== "0010110210")
            throw new Error("bad result");
    }
    foo(null);
})();
