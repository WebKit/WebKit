//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
"use strict";

function assert(b, m) {
    if (!b)
        throw new Error("Bad:" + m);
}
noInline(assert);

function foo(p) {
    function C() {
        this.y = 42;
    }
    C.prototype = p;
    let result = new C;
    return result;
}

function bar(p) {
    function C() {
        this.y = 42;
    }
    C.prototype = p;
    let result = new C;
    return result;
}

function access(item) {
    return item.x;
}

function makeLongChain(x) {
    let item = {x:42};
    for (let i = 0; i < x; ++i) {
        item = {__proto__:item}
    }
    return item;
}


let p1 = makeLongChain(10);
let a = foo(p1);
let b = bar(p1);
b.__proto__ = makeLongChain(10);
function accessY(x) { return x.y; }
accessY(a);
accessY(b);
accessY(a);
accessY(b);

let start = Date.now();
for (let i = 0; i < 10000; ++i) {
    let a = foo(p1);
    for (let i = 0; i < 1000; ++i) {
        assert(a.x === 42);
    }
    let proto = {x:42};
    let b = bar(proto);
    for (let i = 0; i < 100; ++i) {
        assert(b.x === 42);
    }
}

if (false)
    print(Date.now() - start);
