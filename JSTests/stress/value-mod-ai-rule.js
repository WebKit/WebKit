function assert(a, e) {
    if (a !== e)
        throw new Error("Expected: " + e + " bug got: " + a);
}

(() => {
    let predicate = true;
    function foo(a) {
        let v = a;
        if (predicate)
            v = 10;

        let c = v % 2;
        return c;
    }
    noInline(foo);

    for (let i = 0; i < 10000; i++) {
        assert(foo("10"), 0);
    }
})();

(() => {
    let predicate = true;
    function foo(a) {
        let v = a;
        if (predicate)
            v = 10.5;

        let c = v % 2;
        return c;
    }
    noInline(foo);

    for (let i = 0; i < 10000; i++) {
        assert(foo("10"), 0.5);
    }
})();

