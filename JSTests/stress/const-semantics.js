"use strict";
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


// ========== tests below ===========

const NUM_LOOPS = 1000;


;(function() {
    function foo() {
        const x = 40;
        const {y} = {y: 50}, [z] = [60];
        assert(x === 40);
        assert(y === 50);
        assert(z === 60);
    }
    function bar() {
        const x = 40;
        const {y} = {y: 50}, [z] = [60];
        function capture() { return x + y + z; }
        assert(x === 40);
        assert(y === 50);
        assert(z === 60);
        assert(capture() === 150);
        if (truth()) {
            const x = "x";
            assert(x === "x");
            if (truth()) {
                let x = 100;
                const y = 200;
                assert(x === 100);
                assert(y === 200);

                x = 10;
                assert(x === 10);
            }
            assert(x === "x");
        }
        assert(x === 40);
    }
    function baz() {
        let y = 10;
        function sideEffects() {
            y = 20;
        }

        const x = 10;
        try {
            x = sideEffects();
        } catch(e) {}
        assert(y === 20);
        assert(x === 10);

        try {
            x = y = 30;
        } catch(e) {}
        assert(y === 30);
        assert(x === 10);
    }
    function taz() {
        let y = 10;
        let z;
        function sideEffects() {
            y = 20;
            return ["hello", "world"];
        }

        const x = 10;
        try {
            [z, x] = sideEffects();
        } catch(e) {}
        assert(y === 20);
        assert(x === 10);
        assert(z === "hello");
    }
    function jaz() {
        const x = "x";
        function capX() { return x; }
        assert(x === "x");
        assert(capX() === "x");
        if (truth()) {
            let y = 40;
            let capY = function() { return y; }
            assert(x === "x");
            assert(capX() === "x");
        }
        assert(x === "x");
        assert(capX() === "x");
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        foo();
        bar();
        baz();
        jaz();
    }
})();


;(function() {
    function foo() {
        const x = 40;
        x = 30;
    }
    function bar() {
        const x = 40;
        function capX() { return x; }
        x = 30;
    }
    function baz() {
        const x = 40;
        assert(x === 40);
        function bad() { x = 10; }
        bad();
    }
    function jaz() {
        const x = 40;
        assert(x === 40);
        function bad() { eval("x = 10"); }
        bad();
    }
    function faz() {
        const x = 40;
        assert(x === 40);
        eval("x = 10");
    }
    function raz() {
        const x = 40;
        assert(x === 40);
        ;({x} = {x: 20});
    }
    function taz() {
        const x = 40;
        function capX() { return x; }
        assert(capX() === 40);
        ;({x} = {x: 20});
    }
    function paz() {
        const x = 20;
        const y = x = 20;
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        shouldThrowInvalidConstAssignment(foo);
        shouldThrowInvalidConstAssignment(bar);
        shouldThrowInvalidConstAssignment(baz);
        shouldThrowInvalidConstAssignment(jaz);
        shouldThrowInvalidConstAssignment(faz);
        shouldThrowInvalidConstAssignment(raz);
        shouldThrowInvalidConstAssignment(taz);
        shouldThrowInvalidConstAssignment(paz);
    }
})();


;(function() {
    function foo() {
        const x = 40;
        eval("x = 30;");
    }
    function bar() {
        const x = 20;
        x += 20;
    }
    function baz() {
        const x = 20;
        x -= 20;
    }
    function taz() {
        const x = 20;
        shouldThrowInvalidConstAssignment(function() { x = 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x += 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x -= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x *= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x /= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x >>= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x <<= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x ^= 20; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x++; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { x--; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { ++x; });
        assert(x === 20);
        shouldThrowInvalidConstAssignment(function() { --x; });
        assert(x === 20);
    }
    function jaz() {
        const x = 20;
        let threw = false;
        try { x = 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x += 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x -= 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x *= 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x /= 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x >>= 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x <<= 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x ^= 20; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { x++; } catch(e) { threw = true}
        assert(threw);


        threw = false;
        try { x--; } catch(e) { threw = true}
        assert(threw);


        threw = false;
        try { ++x; } catch(e) { threw = true}
        assert(threw);

        threw = false;
        try { --x; } catch(e) { threw = true};
        assert(threw);
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        shouldThrowInvalidConstAssignment(foo);
        shouldThrowInvalidConstAssignment(bar);
        shouldThrowInvalidConstAssignment(baz);
        taz();
        jaz();
    }
})();
