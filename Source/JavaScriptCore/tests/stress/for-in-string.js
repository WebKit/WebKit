(function() {
    // Iterate over characters in a string.
    var o = "hello";
    var foo = function(o) {
        var result = "";
        for (var s in o)
            result += o[s];
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo("hello") !== "hello")
            throw new Error("incorrect result");
    }
    foo(null);
})();
