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

// Test cases

const NUM_LOOPS = 1000;
const SHORT_LOOPS = 100;

;(function() {
function foo() {
    x;
    const x = 20;
}
function bar() {
    const x = x;
}
function baz() {
    const {x: prop, y: prop2} = {x: prop};
}
function jaz() {
    const {x: prop, y: prop2} = {x: 20, y: prop};
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
    const x = 20;
    function captureX() { return x; }
}
function bar() {
    captureX();
    const x = 20;
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
        const x = 20;
        assert(x === 20);
    }
    x;
    const x = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();



;(function() {
function foo() {
    if (truth()) {
        const y = 20;
        const captureY = function() { return y; }
        assert(y === 20);
        x;
    }
    const x = 20;
    const y = 40;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    if (truth()) {
        const y = 20;
        const x = 40;
        const captureAll = function() { return x + y; }
        assert(y === 20);
        assert(x === 40);
        assert(captureAll() === 60);
    }
    tdz;
    const tdz = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    if (truth()) {
        const y = 20;
        const x = 40;
        const captureAll = function() { return x + y + tdz; }
        assert(y === 20);
        assert(x === 40);
    }
    tdz;
    const tdz = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    x = 10;
    const x = 20;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    x = 10;
    const x = 20;
    function cap() { return x; }
}
function bar() {
    captureX();
    const x = 20;
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
    const y = undefined;
    assert(y === undefined);
    if (truth()) {
        x;
    }
    const x = undefined;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    eval("x;");
    const x = 20;
}
function bar() {
    function captureX() { return x; }
    eval("captureX();");
    const x = 20;
}
function baz() {
    function captureX() { return x; }
    function other() { return captureX; }
    other()();
    const x = 20;
}

for (var i = 0; i < SHORT_LOOPS; i++) {
    shouldThrowTDZ(foo);
    shouldThrowTDZ(bar);
    shouldThrowTDZ(baz);
}
})();


;(function() {
function foo() {
    const y = 40;
    eval("y; x;");
    const x = 20;
}

for (var i = 0; i < 1; i++) {
    shouldThrowTDZ(foo);
}
})();


;(function() {
function foo() {
    const x = 20;
    const y = 40;
    assert(eval("x;") === 20);
    if (truth()) {
        assert(eval("eval('y');") === 40);
        eval("eval('x');");
        const x = 40;
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
    const x = function() { return 20; };
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
    const x = function() { return 20; };
}

function bar() {
    typeof x;
    const x = function() { return 20; };
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
    const x = 20;
}

function bar() {
    x--;
    const x = 30;
}

function baz() {
    x *= 40;
    const x = 30;
}

function kaz() {
    function captureX() { return x; }
    x /= 20;
    const x = 20;
}

function haz() {
    function captureX() { return x; }
    --x;
    const x = 20;
}

function jaz() {
    --x;
    const x = 30;
}

for (var i = 0; i < NUM_LOOPS; i++) {
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
    const y = 50;
    let result = null;
    switch(x) {
        case 10:
            const y = 40;
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
    const y = 50;
    let result = null;
    switch(x) {
        case 10:
            const y = 40;
            assert(y === 40);
        case 20:
            const capY = function() { return y; }
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
    const y = 50;
    let result = null;
    switch(x) {
        case 10:
            const y = 40;
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
})();


;(function() {
function foo() {
    [x] = [1];
    const x = null;
}

function boo() {
    [x] = [1];
    const x = 20;
    function capX() { return x; }
}

function bar() {
    ({a, p: y} = {a: 100, p: 40});
    const y = 40;
}

function zar() {
    ({a, p: y} = {a: 100, p: 40});
    const y = 10;
    function capY() { return y; }
}

function baz() {
    ({a, p: {y}} = {a: 100, p: {p: {y: 40}}});
    const y = 100;
}

function jaz() {
    ({y} = {});
    const y = null;
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
