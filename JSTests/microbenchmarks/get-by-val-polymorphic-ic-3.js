function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test3() {
    function foo(o, i) {
        return o[i];
    }
    noInline(foo);

    function args(a, b) {
        return arguments;
    }

    let o1 = args(20, 30);
    let o2 = [10];
    let o3 = {x:42}

    let start = Date.now();
    for (let i = 0; i < 8000000; ++i) {
        assert(foo(o1, 0) === 20);
        assert(foo(o1, 1) === 30);
        assert(foo(o2, 0) === 10);
        assert(foo(o3, "x") === 42);
    }
}
test3();
