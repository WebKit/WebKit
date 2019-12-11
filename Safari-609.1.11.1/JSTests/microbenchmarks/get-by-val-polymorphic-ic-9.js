function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test7() {
    function foo(o, i) {
        return o[i];
    }
    noInline(foo);

    let s1 = Symbol();
    let s2 = Symbol();
    let o = {[s1]: 42, [s2]: 43, o:32};
    o[0] = 1337;
    let start = Date.now();
    for (let i = 0; i < 10000000; ++i) {
        assert(foo(o, s1) === 42);
        assert(foo(o, s2) === 43);
        assert(foo(o, "o") === 32);
        assert(foo(o, 0) === 1337);
    }
}
test7();
