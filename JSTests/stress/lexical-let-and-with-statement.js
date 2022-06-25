function truth() {
    return true;
}
noInline(truth);

function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}

noInline(assert);

;(function() {
    function foo() {
        let x = 40;
        with ({x : 100}) {
            assert(x === 100);
        }
        with ({y : 100}) {
            assert(x === 40);
        }
    }
    noInline(foo);

    function bar() {
        let x = 40;
        function capX() { return x; }
        with ({x : 100}) {
            if (truth()) {
                let x = 50;
                let capX = function() { return x; }
                assert(x === 50);
                assert(capX() === x);
            }
            assert(x === 100);
            assert(capX() === 40);
        }
        with ({y : 100}) {
            if (truth()) {
                let x = 50;
                let capX = function() { return x; }
                assert(x === 50);
                assert(capX() === x);
            }
            assert(x === 40);
            assert(capX() === 40);
        }
    }
    noInline(bar);

    function baz() {
        let x = 40;
        function capX() { return x; }
        with ({x : 100}) {
            if (truth()) {
                let x = 50;
                assert(x === 50);
            }
            assert(x === 100);
            assert(capX() === 40);
        }
        with ({y : 100}) {
            if (truth()) {
                let x = 50;
                assert(x === 50);
            }
            assert(x === 40);
            assert(capX() === 40);
        }
    }
    noInline(baz);

    for (let i = 0; i < 100; i++) {
        foo();
        bar();
        baz();
    }
})();
