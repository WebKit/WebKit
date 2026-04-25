function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}

(function() {
    {
        function* foo() {}

        if (true) {
            function* bar() {}
        }
    }

    assert(typeof foo === "undefined");
    assert(typeof bar === "undefined");
})();

eval(`
    if (true) {
        function* foo1() {}
    }

    {
        function* bar1() {}
    }
`);

assert(!globalThis.hasOwnProperty("foo1"));
assert(!globalThis.hasOwnProperty("bar1"));
