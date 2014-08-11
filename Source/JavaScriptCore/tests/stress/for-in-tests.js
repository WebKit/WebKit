(function() {
    // Iterate over an array with normal indexed properties.
    var foo = function() {
        var a = [1, 2, 3, 4, 5];
        var sum = 0;
        var result = "";
        for (var p in a)
            result += a[p];
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "12345")
            throw new Error("bad result");
    }
    foo(null);
})();
(function() {
    // Iterate over an object with normal non-indexed properties.
    var foo = function() {
        var o = {};
        o.x = 1;
        o.y = 2;
        o.z = 3;
        var result = "";
        for (var p in o)
            result += o[p];
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "123")
            throw new Error("bad result");
    }
    foo(null);
})();
(function() {
    // Iterate over an object with both indexed and non-indexed properties.
    var foo = function() {
        var o = {};
        o.x = 1;
        o.y = 2;
        o.z = 3;
        o[0] = 4;
        o[1] = 5;
        o[2] = 6;
        var result = "";
        for (var p in o)
            result += o[p];
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() != "456123")
            throw new Error("bad result");
    }
    foo(null);
})();
(function() {
    // Iterate over an array with both indexed and non-indexed properties.
    var foo = function() {
        var a = [4, 5, 6];
        a.x = 1;
        a.y = 2;
        a.z = 3;
        var result = "";
        for (var p in a)
            result += a[p];
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "456123")
            throw new Error("bad result");
    }
    foo(null);
})();
