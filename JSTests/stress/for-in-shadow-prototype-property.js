(function() {
    // Add a property to the base object that shadows a property in the prototype during iteration.
    var foo = function() {
        var A = function() {};
        A.prototype.x = "A.x";
        A.prototype.y = "A.y";
        var o = new A();
        var result = "";
        for (var p in o) {
            if (p == "x")
                o.y = "o.y";
            result += o[p];
        }
        return result;
    };
    noInline(foo);
    for (var i = 0; i < 10000; ++i) {
        if (foo() !== "A.xo.y")
            throw new Error("bad result");
    }
    foo(null);
})();
