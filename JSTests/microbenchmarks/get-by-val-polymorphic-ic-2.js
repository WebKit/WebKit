function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test2() {
    function foo(o, i) {
        return o[i];
    }
    noInline(foo);

    let t1 = {};
    let t2 = {};

    let o1 = [t1];
    let o2 = [10];
    let o3 = [10.5];
    let o4 = [t2];
    let o5 = {x:42}
    ensureArrayStorage(o4);

    let start = Date.now();
    for (let i = 0; i < 8000000; ++i) {
        assert(foo(o1, 0) === t1);
        assert(foo(o2, 0) === 10);
        assert(foo(o3, 0) === 10.5);
        assert(foo(o4, 0) === t2);
        assert(foo(o5, "x") === 42);
        assert(foo(o5, "x") === 42);
    }
}
test2();
