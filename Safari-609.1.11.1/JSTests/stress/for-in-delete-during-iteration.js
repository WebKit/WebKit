(function() {
    // Remove a yet-to-be-visited indexed property during iteration.
    var foo = function() {
        var a = [1, 2, 3, 4, 5];
        var result = "";
        for (var p in a) {
            if (p == 2)
                delete a[3];
            result += a[p];
        }
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "1235")
            throw new Error("bad result");
    }
    foo(null);
})();
(function() {
    // Remove a yet-to-be-visited non-indexed property during iteration.
    var foo = function() {
        var o = {};
        o.x = "x";
        o.y = "y";
        o.z = "z";
        var result = "";
        for (var p in o) {
            if (p == "x") {
                delete o.y;
                o.a = "a";
            }
            result += o[p];
        }
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "xz")
            throw new Error("bad result");
    }
})();
(function() {
    // Remove then re-add a property during iteration.
    var foo = function() {
        var A = function() {};
        A.prototype.x = "A.x";
        A.prototype.y = "A.y";
        var o = new A();
        o.z = "o.z";
        o.y = "o.y";
        o.x = "o.x";
        var result = "";
        for (var p in o) {
            if (p == "z")
                delete o.x;
            if (p == "y")
                o.x = "o.x";
            result += o[p];
        }
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "o.zo.yo.x")
            throw new Error("bad result");
    }
    foo(null);
})();
