//@ $skipModes << :lockdown if $buildType == "debug"

function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test1() {
    function foo(o, i) {
        return o[i];
    }
    noInline(foo);

    let o = {a: 42, b: 43};
    let start = Date.now();
    for (let i = 0; i < 10000000; ++i) {
        assert(foo(o, "a") === 42);
        assert(foo(o, "b") === 43);
        assert(foo(o, "c") === undefined);
        assert(foo(o, "d") === undefined);
    }
}
test1();
