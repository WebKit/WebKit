function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test6() {
    function foo(o, i) {
        return o[i];
    }
    noInline(foo);

    function args(a, b) {
        function capture() { 
            return a + b;
        }
        return arguments;
    }

    let o1 = "abc";
    let o2 = "\u2713a";
    let o3 = new Uint32Array([3, 4]);

    let start = Date.now();
    for (let i = 0; i < 2000000; ++i) {
        assert(foo(o1, 0) === "a");
        assert(foo(o1, 1) === "b");
        assert(foo(o1, 2) === "c");
        assert(foo(o2, 0) === "\u2713");
        assert(foo(o2, 1) === "a");
        assert(foo(o3, 0) === 3);
        assert(foo(o3, 1) === 4);
    }
}
test6();
