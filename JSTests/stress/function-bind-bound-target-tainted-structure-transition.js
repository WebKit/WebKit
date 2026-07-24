//@ requireOptions("--forceDiskCache=0")
// $vm doesn't have a source provider that can cache to disk. Skip that testing configuration.

$vm.runTaintedString(`
    function shouldBe(actual, expected) {
        if (actual !== expected)
            throw new Error("bad value: " + actual);
    }

    function target() { }

    function test(mode, count) {
        var result = null;
        for (var i = 0; i < count; ++i) {
            var o;
            if (mode === 0)
                o = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7, h: 8, i: 9, j: 10, k: 11 };
            else {
                o = target.bind();
                if (mode === 2)
                    o.bind();
            }
            o.x = 42;
            if (mode === 2)
                result = o;
        }
        return result;
    }

    for (var i = 0; i < 80; ++i) {
        test(1, 1000);
        test(0, 1000);
        test(2, 100);
    }
    test(1, 80000);

    var boundFunction = test(2, 1);
    shouldBe(typeof boundFunction, "function");
    shouldBe(boundFunction.name, "bound target");
    shouldBe(boundFunction.hasOwnProperty("a"), false);
    shouldBe(boundFunction.x, 42);
`);
