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
        const obj = {a: 20, b: 40, c: 50};
        const props = [];
        for (const p in obj)
            props.push(p);
        assert(props.length === 3);
        for (const p of props)
            assert(!!obj[p]);
        assert(props.indexOf("a") !== -1);
        assert(props.indexOf("b") !== -1);
        assert(props.indexOf("c") !== -1);
    }

    function bar() {
        const funcs = [];
        for (const item of [1, 2, 3])
            funcs.push(function() { return item; });
        let i = 1;
        assert(funcs.length === 3);
        for (const f of funcs) {
            assert(f() === i);
            i++;
        }
    }
    function baz() {
        const funcs = [];
        const obj = {a:1, b:2, c:3};
        for (const p in obj)
            funcs.push(function() { return p; });
        let i = 1;
        assert(funcs.length === 3);
        for (const f of funcs) {
            assert(obj[f()] === i);
            i++;
        }
    }
    function taz() {
        for (const item of [1,2,3]) {
            const item = 20;
            assert(item === 20);
        }
    }
    function jaz() {
        let i = 0;
        for (const x = 40; i < 10; i++) {
            assert(x === 40);
        }
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        foo();
        bar();
        baz();
        taz();
        jaz();
    }
})();

;(function() {
    function foo() {
        for (const item of [1,2,3])
            item = 20;
    }
    function bar() {
        for (const item of [1,2,3])
            eval("item = 20")
    }
    function baz() {
        for (const p in {a: 20, b: 40})
            p = 20;
    }
    function taz() {
        for (const p in {a: 20, b: 40})
            eval("p = 20")
    }
    function jaz() {
        for (const x = 0; x < 10; x++) { }
    }
    function raz() {
        for (const x = 0; x < 10; ++x) { }
    }
    function paz() {
        for (const x = 0; x < 10; x++) { 
            let f = function() { return x; }
        }
    }
    function maz() {
        for (const x = 0; x < 10; ++x) { 
            let f = function() { return x; } 
        }
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        shouldThrowInvalidConstAssignment(foo);
        shouldThrowInvalidConstAssignment(bar);
        shouldThrowInvalidConstAssignment(baz);
        shouldThrowInvalidConstAssignment(taz);
        shouldThrowInvalidConstAssignment(jaz);
        shouldThrowInvalidConstAssignment(raz);
        shouldThrowInvalidConstAssignment(paz);
        shouldThrowInvalidConstAssignment(maz);
    }
})();
