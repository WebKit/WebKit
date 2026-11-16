function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

for (var i = 0; i < 1000; i++) {
    ;(function foo() {
        foo = 20;
        assert(foo !== 20);
        assert(typeof foo === "function");
    })();

    ;(function foo() {
        var bar = function() { return foo; }
        foo = 20;
        assert(foo !== 20);
        assert(bar() !== 20);
        assert(typeof foo === "function");
        assert(typeof bar() === "function");
    })();

    ;(function foo() {
        eval("foo = 20;");
        assert(foo !== 20);
        assert(typeof foo === "function");
    })();

    ;(function foo() {
        eval("var foo = 20;");
        assert(foo === 20);
    })();

    ;(function foo() {
        "use strict";
        assert(foo !== 20);
        assert(typeof foo === "function");
    })();
}
