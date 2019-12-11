
function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

for (var i = 0; i < 1000; i++) {

    ;(function foo(x = 20) {
        assert(typeof foo === "function");
    })();

    ;(function foo(x = 20) {
        function bar() { return foo; }
        assert(typeof foo === "function");
    })();

    ;(function foo(x = foo) {
        var foo = 20;
        assert(foo === 20);
        assert(typeof x === "function");
    })();

    ;(function foo(capFoo = function() { return foo; }) {
        var foo = 20;
        assert(foo === 20);
        assert(typeof capFoo() === "function");
    })();

    ;(function foo(x = eval("foo")) {
        var foo = 20;
        assert(foo === 20);
        assert(typeof x === "function");
    })();
}
