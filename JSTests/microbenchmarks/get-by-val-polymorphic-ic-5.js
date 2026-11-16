//@ $skipModes << :lockdown if $buildType == "debug"

function assert(b, m) {
    if (!b)
        throw new Error(m);
}

function test5() {
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

    let o1 = new Uint8Array([1, 2]);
    let o2 = new Uint32Array([3, 4]);
    let o3 = new Int32Array([5, 6]);
    let o4 = new Float32Array([7, 8]);

    let start = Date.now();
    for (let i = 0; i < 8000000; ++i) {
        assert(foo(o1, 0) === 1);
        assert(foo(o1, 1) === 2);
        assert(foo(o2, 0) === 3);
        assert(foo(o2, 1) === 4);
        assert(foo(o3, 0) === 5);
        assert(foo(o3, 1) === 6);
        assert(foo(o4, 0) === 7);
        assert(foo(o4, 1) === 8);
    }
}
test5();
