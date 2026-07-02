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
    let o = {[s1]: 42, [s2]: 43};
    let start = Date.now();
    for (let i = 0; i < 10000000; ++i) {
        assert(foo(o, s1) === 42);
        assert(foo(o, s2) === 43);
    }
}
test7();
