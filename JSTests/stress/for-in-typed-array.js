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

(function() {
    function forIn() {
        var a = new Int32Array(4);
        a.foo = 1;
        a.bar = 2;
        for (var i = 0; i < a.length; ++i)
            a[i] = i;

        var keys = [];
        for (var k in a)
            keys.push(k);
        return keys.join("|");
    }
    noInline(forIn);

    for (var i = 0; i < 1e4; ++i) {
        var keys = forIn();
        if (keys !== "0|1|2|3|foo|bar")
            throw new Error(`Bad result: ${keys}`);
    }
})();
