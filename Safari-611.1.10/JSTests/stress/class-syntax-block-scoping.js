function assert(b) {
    if (!b)
        throw new Error("Assertion failure");
}
noInline(assert);

function truth() { return true; }
noInline(truth);

const NUM_LOOPS = 1000;

;(function() {
    function foo() {
        let first;
        let second;
        class A {};
        first = A;
        if (truth()) {
            class A {};
            second = A;
        }
        assert(first !== second);
    }
    function baz() {
        class A { static hello() { return 10; } };
        assert(A.hello() === 10);
        if (truth()) {
            class A { static hello() { return 20; } };
            assert(A.hello() === 20);
        }
        assert(A.hello() === 10);
    }
    function bar() {
        class A { static hello() { return 10; } };
        let capA = function() { return A; }
        assert(A.hello() === 10);
        if (truth()) {
            class A { static hello() { return 20; } };
            let capA = function() { return A; }
            assert(A.hello() === 20);
        }
        assert(A.hello() === 10);
    }
    for (let i = 0; i < NUM_LOOPS; i++) {
        foo();
        bar();
        baz();
    }
})();
