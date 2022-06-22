//@ requireOptions("--getByValICMaxNumberOfIdentifiers=2")

let program = `
    function shouldBe(actual, expected) {
        if (actual !== expected)
            throw new Error('bad value: ' + actual);
    }
    noInline(shouldBe);

    function foo(o, p) {
        return o[p];
    }
    noInline(foo);

    function runMono() {
        let o = {
            get x() {
                if ($vm.ftlTrue()) OSRExit();
                return 42;
            }
        };
        for (let i = 0; i < 1000000; ++i) {
            shouldBe(foo(o, "x"), 42);
        }
    }

    function runPoly() {
        let o = {
            a: 1,
            b: 2,
            c: 4,
            d: 4,
            e: 4,
            f: 4,
            g: 4,
        };
        for (let i = 0; i < 1000000; ++i) {
            foo(o, "a");
            foo(o, "b");
            foo(o, "c");
            foo(o, "d");
            foo(o, "e");
            foo(o, "f");
            foo(o, "g");
            foo(o, "h");
            foo(o, "i");
        }
    }
`;

let g1 = runString(program);
g1.runPoly();

let g2 = runString(program);
g2.runMono();
