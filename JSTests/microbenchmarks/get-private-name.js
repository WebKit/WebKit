//@ $skipModes << :lockdown if $buildType == "debug"

function assert(b, m = "Assertion failed") {
    if (!b)
        throw new Error(m);
}

function test1() {
    function factory(i) {
        return new class {
            #x = i;
            get() { return this.#x; }
        };
    }

    function foo(o, i) {
        return o.get();
    }
    noInline(foo);

    let a = factory(42);
    let b = factory(43);
    let start = Date.now();
    for (let i = 0; i < 10000000; ++i) {
        assert(foo(a, "a") === 42);
        assert(foo(b, "b") === 43);
    }
}
test1();
