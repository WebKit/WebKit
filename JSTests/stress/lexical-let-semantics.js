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


// ========== tests below ===========

const NUM_LOOPS = 1000;

let globalLet = "helloWorld";
assert(globalLet === "helloWorld");
function captureGlobalLet() { return globalLet; }
assert(globalLet === captureGlobalLet());
let globalFunction = function() { return 20; }
assert(globalFunction() === 20);
assert((function() { return globalFunction(); })() === 20);
let globalNumber = 20;
assert(globalNumber === 20);
globalNumber++;
assert(globalNumber === 21);
globalNumber += 40;
assert(globalNumber === 61);
globalNumber = "hello";
assert(globalNumber === "hello");

let globalNumberCaptured = 20;
let retGlobalNumberCaptured = function() { return globalNumberCaptured; }
let setGlobalNumberCaptured = function(x) { globalNumberCaptured = x; }
assert(retGlobalNumberCaptured() === globalNumberCaptured);
globalNumberCaptured++;
assert(retGlobalNumberCaptured() === globalNumberCaptured);
assert(globalNumberCaptured === 21);
setGlobalNumberCaptured(100);
assert(retGlobalNumberCaptured() === globalNumberCaptured);
assert(globalNumberCaptured === 100);
setGlobalNumberCaptured(retGlobalNumberCaptured);
assert(retGlobalNumberCaptured() === retGlobalNumberCaptured);
assert(globalNumberCaptured === retGlobalNumberCaptured);

var arrOfFuncs = [];
for (var i = 0; i < NUM_LOOPS; i++) {
    let globalLet = "inner";
    assert(globalLet === "inner");
    let inner = i;
    arrOfFuncs.push(function() { return inner; });
}
assert(globalLet === "helloWorld");
for (var i = 0; i < arrOfFuncs.length; i++)
    assert(arrOfFuncs[i]() === i);


var globalVar = 100;
assert(globalVar === 100);
;(function () {
    assert(globalVar === 100);
    if (truth()) {
        let globalVar = 20;
        assert(globalVar === 20);
    }
    assert(globalVar === 100);
})();

assert(globalVar === 100);
;(function () {
    let globalVar = 10;
    assert(globalVar === 10);
    if (truth()) {
        let globalVar = 20;
        assert(globalVar === 20);
    }
    assert(globalVar === 10);
})();
assert(globalVar === 100);


;(function() {
function foo() {
    let x = 20;
    
    if (truth()) {
        let thingy = function() { 
            x = 200;
            return x; 
        };
        noInline(thingy);
        thingy();
    }

    return x;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    assert(foo() === 200);
}
})();


;(function() {
var arr = [];
function foo(i) {
    var num = i;
    
    if (truth()) {
        let num = i;
        arr.push(function() { return num; });
    }
    var oldFunc = arr[arr.length - 1];
    arr[arr.length - 1] = function() { return oldFunc() + num; }
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo(i);
}

for (var i = 0; i < arr.length; i++) {
    assert(arr[i]() === i + i);
}
})();


;(function() {
function foo() {
    let x = 20;
    let y = 40;
    assert(x === 20);
    assert(y === 40);
    if (truth()) {
        let x = 50;
        let y = 60;
        assert(x === 50);
        assert(y === 60);
    }
    assert(x === 20);
    assert(y === 40);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}
})();


;(function() {
function foo() {
    function captureX() { return x; }
    let x = 20;
    let y = 40;
    assert(x === 20);
    assert(y === 40);
    if (truth()) {
        let x = 50;
        let y = 60;
        assert(x === 50);
        assert(y === 60);
    }
    assert(x === 20);
    assert(y === 40);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}
})();


;(function() {
function foo() {
    let x = 20;
    let y = 40;
    function captureAll() { return x + y; }
    noInline(captureAll);
    assert(x === 20);
    assert(y === 40);
    assert(captureAll() === 60);
    if (truth()) {
        let x = 50;
        assert(x + y === 90);
        assert(captureAll() === 60);
    }
    assert(x === 20);
    assert(y === 40);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}
})();


;(function() {
function foo() {
    var captureAll = function() { return x + y; }
    let x = 20;
    let {_y : y, z} = {_y : 40, z : 100};
    assert(x === 20);
    assert(y === 40);
    assert(z === 100);
    assert(captureAll() === 60);
    if (truth()) {
        let x = 50;
        assert(x + y === 90);
        assert(y === 40);
        assert(captureAll() === 60);
    }
    assert(x === 20);
    assert(y === 40);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}
})();


;(function() {
function foo() {
    var captureAll = function() { return x + y; }
    let x = 20;
    let y = 40;
    assert(x === 20);
    assert(y === 40);
    assert(captureAll() === 60);
    if (truth()) {
        let x = 50;
        let secondCaptureAll = function() { return x + y; };
        assert(x + y === 90);
        assert(secondCaptureAll() === 90);
    }
    assert(x === 20);
    assert(y === 40);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}
})();


;(function() {
function foo() {
    let x, y, z;
    assert(x === undefined);
    assert(y === undefined);
    assert(z === undefined);
}
function bar() {
    let x, y, z;
    if (truth()) {
        let x = 20, y = 40;
        assert(x === 20);
        assert(y === 40);
    }
    function capture() { return x + z; }
    assert(x === undefined);
    assert(y === undefined);
    assert(z === undefined);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
    bar();
}
})();


;(function() {
function foo() {
    let x, y, z = "z", t = undefined;
    assert(cap() === undefined);
    assert(x === undefined);
    assert(y === undefined);
    assert(t === undefined);
    assert(z === "z");

    function cap() { return x; }
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}
})();

;(function() {
function foo() {
    let {x: baz} = {x: 20};
    let {x: bar} = {x: 200};
    function cap() { return baz; }
    assert(baz === 20);
    assert(bar === 200);
    assert(cap() === 20);
    baz = 40;
    assert(baz === 40);
    assert(cap() === 40);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}

})();



;(function() {
function foo() {
    let x = 20;
    let y = 50;
    assert(y === 50);
    assert(eval("y = 25; let x = 40; x;") === 40);
    assert(x === 20);
    assert(y === 25);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
}
})();


;(function() {
function foo() {
    let x = 20;
    let y = 50;
    assert(y === 50);
    if (truth()) {
        let y = 30;
        assert(y === 30);
        assert(eval("y = 25; let x = 40; x;") === 40);
        assert(y === 25);
        assert(x === 20);
        if (truth()) {
            let y = 100;
            assert(y === 100);
            x = 1;
        }
        assert(x === 1);
        assert(y === 25);
    }
    assert(x === 1);
    assert(y === 50);
}

for (var i = 0; i < NUM_LOOPS; i++) {
    foo();
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

for (var i = 0; i < NUM_LOOPS; i++) {
    assert(foo(10) === 41);
}

assert(foo("hello") === "hello");
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
            result = function() { return y; };
            break;
        default:
            result = x;
            break;
    }
    assert(y === 50);
    return result;
}

for (var i = 0; i < NUM_LOOPS; i++) {
    assert(foo(10)() === 41);
}

assert(foo("hello") === "hello");
})();
