function truth() {
    return true;
}
noInline(truth);

function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

function shouldThrowInvalidConstAssignment(f) {
    var threw = false;
    try {
        f();
    } catch(e) {
        if (e.name.indexOf("TypeError") !== -1 && e.message.indexOf("readonly") !== -1)
            threw = true;
    }
    assert(threw);
}
noInline(shouldThrowInvalidConstAssignment);


// Tests

const NUM_LOOPS = 100;

;(function() {
    function foo() {
        const x = 40;
        with ({x : 100}) {
            assert(x === 100);
        }
        with ({y : 100}) {
            assert(x === 40);
        }
    }
    noInline(foo);

    function bar() {
        const x = 40;
        function capX() { return x; }
        with ({x : 100}) {
            if (truth()) {
                const x = 50;
                const capX = function() { return x; }
                assert(x === 50);
                assert(capX() === x);
            }
            assert(x === 100);
            assert(capX() === 40);
        }
        with ({y : 100}) {
            if (truth()) {
                const x = 50;
                const capX = function() { return x; }
                assert(x === 50);
                assert(capX() === x);
            }
            assert(x === 40);
            assert(capX() === 40);
        }
    }
    noInline(bar);

    function baz() {
        const x = 40;
        function capX() { return x; }
        with ({x : 100}) {
            if (truth()) {
                const x = 50;
                assert(x === 50);
            }
            assert(x === 100);
            assert(capX() === 40);
        }
        with ({y : 100}) {
            if (truth()) {
                const x = 50;
                assert(x === 50);
            }
            assert(x === 40);
            assert(capX() === 40);
        }
    }
    noInline(baz);

    for (let i = 0; i < NUM_LOOPS; i++) {
        foo();
        bar();
        baz();
    }
})();


;(function() {
    function foo() {
        const x = 40;
        with ({x : 100}) {
            assert(x === 100);
            x = 20;
            assert(x === 20);
        }
        assert(x === 40);
        with ({y : 100}) {
            assert(x === 40);
            x = 100;
        }
    }
    for (let i = 0; i < NUM_LOOPS; ++i) {
        shouldThrowInvalidConstAssignment(foo);
    }

})();
