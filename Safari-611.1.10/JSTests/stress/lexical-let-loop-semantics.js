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

const NUM_LOOPS = 1000;
const SHORT_LOOPS = 100;

function shouldThrowTDZ(func) {
    var hasThrown = false;
    try {
        func();
    } catch(e) {
        if (e.name.indexOf("ReferenceError") !== -1)
            hasThrown = true;
    }
    assert(hasThrown);
}
noInline(shouldThrowTDZ);


;(function () {
    var arr = [];

    for (let i = 0; i < 10; i++) {
        arr.push(function () { return i; })
    }

    for (let j = 0; j < arr.length; j++) {
        assert(arr[j]() === j);
    }

    let f = "fff";
    let counter = 0;
    for (let f of arr) {
        assert(f() === counter++);
    }
    assert(f === "fff");

    let numLoops = 0;
    for (let f of arr) {
        numLoops++;
        let f = function () { return "f"; }
        assert(f() === "f");
    }
    assert(numLoops === arr.length);
    assert(f === "fff");

})();


;(function() {
    function foo() {
        var obj = {hello:1, world:2};
        obj["bar"] = 3;
        let p = 20;
        assert(p === 20);
        for (let p in obj) {
            assert(p === "hello" || p === "world" || p === "bar");
        }
        assert(p === 20);
    }

    function bar() {
        var obj = {hello:1, world:2};
        obj["bar"] = 3;
        let props = [];
        let p = 20;
        for (let p in obj) {
            props.push(function foo() { return p; });
            let outerP = p;
            if (truth()) {
                let p = 100;
                assert(p === 100);
                assert(p !== outerP);
                assert(outerP === "hello" || outerP === "world" || outerP === "bar");
            }
            assert(p === outerP);
        }
        assert(p === 20);

        let seenProps = {};
        for (let f of props) {
            let p = f();
            assert(p === "hello" || p === "world" || p === "bar");
            seenProps[p] = true;
        }
        assert(seenProps["hello"] === true);
        assert(seenProps["world"] === true);
        assert(seenProps["bar"] === true);
        assert(p === 20);
    }

    for (var i = 0; i < NUM_LOOPS; i++) {
        foo();
        bar();
    }
})();


;(function() {
    function foo() {
        let counter = 0;
        for (let idx = 0; idx < 20; idx++) {
            assert(idx === counter++);
            continue;
        }
        shouldThrowTDZ(function() { return idx; });
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        foo();
    }
})();


;(function() {
    function foo() {
        for (let idx = 0; !truth(); idx++) { }
        shouldThrowTDZ(function() { return idx; }); // Just plain old reference error here.
    }
    function bar() {
        for (let j = 0; j < 20; j++) { 
            if (j === 1)
                break;
            else
                continue;
        }
        shouldThrowTDZ(function() { return j; });
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        foo();
        bar();
    }
})();


;(function() {
    function foo() {
        var obj = {hello:1, world:2};
        let p = 20;
        var arr = []
        for (let p in obj) {
            arr.push(function capP() { return p; });
            assert(p === "hello" || p === "world");
        }
        assert(arr[0]() === "hello" || arr[0]() === "world");
        assert(arr[1]() === "hello" || arr[1]() === "world");
        assert(arr[1]() !== arr[0]());
        assert(p === 20);
    }

    function bar() {
        var obj = {a:1, b:2, c:3, d:4, e:4};
        obj["f"] = 5;
        let funcs = [];
        for (let p in obj) {
            funcs.push(function capP() { return p; });
        }
        let counter = 0;
        for (let p in obj) {
            assert(funcs[counter]() === p);
            counter++;
        }
    }

    for (var i = 0; i < NUM_LOOPS; i++) {
        foo();
        bar();
    }
})();


;(function() {
    function foo() {
        let arr = [0, 1, 2, 3, 4, 5];
        let funcs = [];
        for (let x of arr) {
            funcs.push(function() { return x; });
        }
        for (let i = 0; i < arr.length; ++i) {
            assert(funcs[i]() === i);
        }
    }

    for (var i = 0; i < NUM_LOOPS; i++) {
        foo();
    }
})();


;(function() {
    function foo() {
        let thing = {};
        for (let thing = thing; !thing; ) {}
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        shouldThrowTDZ(foo);
    }
})();


;(function() {
    function foo() {
        let thing = {};
        for (let thing = eval("thing"); !truth(); ) {}
    }

    for (var i = 0; i < SHORT_LOOPS; i++) {
        shouldThrowTDZ(foo);
    }
})();


;(function() {
    function foo() {
        let thing = {};
        for (let thing in thing) {}
    }
    function bar() {
        let thing = {hello: "world"}
        for (let thing in thing) {}
    }
    function baz() {
        let thing = {};
        for (let thing in eval("thing")) {}
    }
    function bag() {
        let thing = {hello: "world"}
        for (let thing in eval("thing")) {}
    }
    for (var i = 0; i < SHORT_LOOPS; i++) {
        shouldThrowTDZ(foo);
        shouldThrowTDZ(bar);
        shouldThrowTDZ(baz);
        shouldThrowTDZ(bag);
    }
})();


;(function() {
    function foo() {
        let thing = ["hello"];
        for (let thing in thing) {}
    }
    function bar() {
        let thing = [];
        for (let thing in thing) {}
    }
    function baz() {
        let thing = {hello: "world"};
        for (let thing in thing) {}
    }
    function bag() {
        let empty = {};
        for (let thing in empty) {}
        return thing;
    }
    function hat() {
        let notEmpty = {foo: "bar"};
        for (let thing in notEmpty) {
            break;
        }
        return thing;
    }
    function cap() {
        let notEmpty = {foo: "bar"};
        for (let thing in notEmpty) {
            continue;
        }
        return thing;
    }
    for (var i = 0; i < NUM_LOOPS; i++) {
        shouldThrowTDZ(foo);
        shouldThrowTDZ(bar);
        shouldThrowTDZ(baz);
        shouldThrowTDZ(bag);
        shouldThrowTDZ(hat);
        shouldThrowTDZ(cap);
    }
})();


;(function() {
    function foo() {
        let thing = ["hello"];
        for (let thing of thing) {}
    }
    function bar() {
        let thing = [];
        for (let thing of thing) {}
    }
    function baz() {
        let thing = ["world"]
        for (let thing of thing) {}
    }
    function bag() {
        let empty = [];
        for (let thing of empty) {}
        return thing;
    }
    function hat() {
        let notEmpty = ["hello", "world"];
        for (let thing of notEmpty) {
            break;
        }
        return thing;
    }
    function tap() {
        let notEmpty = [10, 20];
        for (let thing of notEmpty) { }
        return thing;
    }
    function cap() {
        let notEmpty = [10, 20];
        for (let thing of notEmpty) {
            continue;
        }
        return thing;
    }
    function pap() {
        let notEmpty = [10, 20];
        for (let thing of notEmpty) {
        }
        return thing;
    }
    for (var i = 0; i < SHORT_LOOPS; i++) {
        shouldThrowTDZ(foo);
        shouldThrowTDZ(bar);
        shouldThrowTDZ(baz);
        shouldThrowTDZ(bag);
        shouldThrowTDZ(hat);
        shouldThrowTDZ(tap);
        shouldThrowTDZ(cap);
        shouldThrowTDZ(pap);
    }
})();


;(function() {
    function foo() {
        let x = 0;
        let arr = [];
        for (let x of (x=2, obj)) { x; }
    }

    function bar() {
        let x = 0;
        let obj = {};
        for (let x in (x=2, obj)) { x; }
    }

    for (var i = 0; i < SHORT_LOOPS; i++) {
        shouldThrowTDZ(foo);
        shouldThrowTDZ(bar);
    }
})();


;(function() {
    let factorial = null;
    function test() {
        for (let factorial = function(x){ return x > 1 ? x * factorial(x - 1) : 1; }; true; ) { return factorial(5); }
    }
    assert(test() === 120);
})();

;(function() {
    function test() {
        for (let factorial = function(x){ return x > 1 ? x * factorial(x - 1) : 1; }; true; ) { return factorial(5); }
    }
    assert(test() === 120);
})();

