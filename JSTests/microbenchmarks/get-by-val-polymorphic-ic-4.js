//@ $skipModes << :lockdown if $buildType == "debug"

function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test4() {
    // scoped arguments
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

    let o1 = args(20, 30, 40);
    let o3 = {x:42}

    let start = Date.now();
    for (let i = 0; i < 8000000; ++i) {
        assert(foo(o1, 0) === 20);
        assert(foo(o1, 1) === 30);
        assert(foo(o1, 2) === 40);
        assert(foo(o1, 3) === undefined);
        assert(foo(o3, "x") === 42);
    }
}
test4();
