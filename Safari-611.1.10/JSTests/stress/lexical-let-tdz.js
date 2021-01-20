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

const NUM_LOOPS = 1000;
const SHORT_LOOPS = 100;


;(function() {
function foo() {
    x;
    let x = 20;
}
function bar() {
    let x = x;
}
function baz() {
    let {x: prop, y: prop2} = {x: prop};
}
function jaz() {
    let {x: prop, y: prop2} = {x: 20, y: prop};
}
for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
    shouldThrowTDZ(baz);
    shouldThrowTDZ(jaz);
}
})();


;(function() {
function foo() {
    x;
    let x = 20;
    function captureX() { return x; }
}
function bar() {
    captureX();
    let x = 20;
    function captureX() { return x; }
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
}
})();


;(function() {
function foo() {
    if (truth()) {
        let x = 20;
        assert(x === 20);
    }
    x;
    let x = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();



;(function() {
function foo() {
    if (truth()) {
        let y = 20;
        let captureY = function() { return y; }
        assert(y === 20);
        x;
    }
    let x = 20;
    let y;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    if (truth()) {
        let y = 20;
        let x = 40;
        let captureAll = function() { return x + y; }
        //noInline(captureAll);
        assert(y === 20);
        assert(x === 40);
        assert(captureAll() === 60);
    }
    tdz;
    let tdz = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    if (truth()) {
        let y = 20;
        let x = 40;
        let captureAll = function() { return x + y + tdz; }
        assert(y === 20);
        assert(x === 40);
    }
    tdz;
    let tdz = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    x = 10;
    let x = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    x = 10;
    let x = 20;
    function cap() { return x; }
}
function bar() {
    captureX();
    let x = 20;
    function captureX() { return x; }
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
}
})();


;(function() {
function foo() {
    if (!truth()) {
        y;
        assert(false);
    }
    let y;
    assert(y === undefined);
    if (truth()) {
        x;
    }
    let x;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    eval("x;");
    let x = 20;
}
function bar() {
    function captureX() { return x; }
    eval("captureX();");
    let x = 20;
}
function baz() {
    function captureX() { return x; }
    function other() { return captureX; }
    other()();
    let x = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
    shouldThrowTDZ(baz);
}
})();


;(function() {
function foo() {
    let y;
    eval("y; x;");
    let x = 20;
}

for (var i = 0; i < SHORT_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    let x = 20;
    let y = 40;
    assert(eval("x;") === 20);
    if (truth()) {
        assert(eval("eval('y');") === 40);
        eval("eval('x');");
        let x = 40;
    }
}

for (var i = 0; i < SHORT_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


// FunctionResolveNode
;(function() {
function foo() {
    function captureX() { return x; }
    x();
    let x = function() { return 20; };
}

function bar() {
    x();
    let x = function() { return 20; };
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
}
})();


// TypeofResolveNode
;(function() {
function foo() {
    function captureX() { return x; }
    typeof x;
    let x = function() { return 20; };
}

function bar() {
    typeof x;
    let x = function() { return 20; };
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
}
})();


// ReadModifyResolveNode
;(function() {
function foo() {
    function captureX() { return x; }
    x++;
    let x = 20;
}

function bar() {
    x--;
    let x = 30;
}

function baz() {
    x *= 40;
    let x = 30;
}

function kaz() {
    function captureX() { return x; }
    x /= 20;
    let x = 20;
}

function haz() {
    function captureX() { return x; }
    --x;
    let x = 20;
}

function jaz() {
    --x;
    let x = 30;
}

for (var i = 0; i < SHORT_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
    shouldThrowTDZ(baz);
    shouldThrowTDZ(kaz);
    shouldThrowTDZ(haz);
    shouldThrowTDZ(jaz);
}
})();


;(function() {
function foo(x) {
    let y = 50;
    let result = null;
    switch(x) {
        case 10:
            let y = 40;
            assert(y === 40);
        case 20:
            y += 1;
            assert(y === 41);
            result = y;
            break;
        default:
            result = x;
            break;
    }
    assert(y === 50);
    return result;
}
function bar(x) {
    let y = 50;
    let result = null;
    switch(x) {
        case 10:
            let y = 40;
            assert(y === 40);
        case 20:
            let capY = function() { return y; }
            y += 1;
            assert(y === 41);
            result = y;
            break;
        default:
            result = x;
            break;
    }
    assert(y === 50);
    return result;
}
function baz(x) {
    let y = 50;
    let result = null;
    switch(x) {
        case 10:
            let y = 40;
            assert(y === 40);
        case 20:
            let inc = function() { y += 1; }
            inc();
            assert(y === 41);
            result = y;
            break;
        default:
            result = x;
            break;
    }
    assert(y === 50);
    return result;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(function() { foo(20); });
    shouldThrowTDZ(function() { bar(20); });
    shouldThrowTDZ(function() { baz(20); });
}

assert(foo(10) === 41);
assert(foo("hello") === "hello");
})();


;(function() {
function foo() {
    let y;
    [y] = [40];
    assert(y === 40);

    [x] = [1];
    let x;
}

function boo() {
    let y;
    [y] = [40];
    assert(y === 40);

    [x] = [1];
    let x;
    function capX() { return x; }
}

function bar() {
    let x;
    ({p: x} = {p: 40});
    assert(x === 40);

    ({a, p: y} = {a: 100, p: 40});
    let y;
}

function zar() {
    let x;
    ({p: x} = {p: 40});
    assert(x === 40);

    ({a, p: y} = {a: 100, p: 40});
    let y;
    function capY() { return y; }
}

function baz() {
    let x;
    ({p: x} = {p: 40});
    assert(x === 40);

    ({a, p: {y}} = {a: 100, p: {p: {y: 40}}});
    let y;
}

function jaz() {
    ({y} = {});
    let y;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(boo);
    shouldThrowTDZ(bar);
    shouldThrowTDZ(zar);
    shouldThrowTDZ(baz);
    shouldThrowTDZ(jaz);
}
})();
