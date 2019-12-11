(function() {
    // Iterate over typed arrays.
    var foo = function() {
        var a = new Uint8Array(5);
        for (var i = 0; i < a.length; ++i)
            a[i] = i;
        var result = "";
        for (var p in a)
            result += a[p];
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "01234")
            throw new Error("bad result");
    }
    foo(null);
})();
