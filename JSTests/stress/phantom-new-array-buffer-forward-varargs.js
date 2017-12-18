"use strict";

function assert(b, m="") {
    if (!b)
        throw new Error("Bad assertion: " + m);
}
noInline(assert);

function test1() {
    function bar(a, b, c, d) {
        return [a, b, c, d];
    }
    function foo() {
        return bar(...[0, 1, 2, 3]);
    }
    noInline(foo);

    for (let i = 0; i < 10000; i++) {
        let [a, b, c, d] = foo();
        assert(a === 0);
        assert(b === 1);
        assert(c === 2);
        assert(d === 3) ;
    }
}

function test2() {
    function bar(...args) {
        return args;
    }
    function foo() {
        let args = [1, 2, 3];
        return bar(...args, 0, ...args);
    }
    noInline(foo);

    for (let i = 0; i < 10000; i++) {
        let r = foo();
        assert(r.length === 7);
        let [a, b, c, d, e, f, g] = r;
        assert(a === 1);
        assert(b === 2);
        assert(c === 3);
        assert(d === 0);
        assert(e === 1);
        assert(f === 2);
        assert(g === 3);
    }
}

function test3() {
    function baz(...args) {
        return args;
    }
    function bar(...args) {
        return baz(...args);
    }
    function foo() {
        let args = [3];
        return bar(...args, 0, ...args);
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++) {
        let r = foo();
        assert(r.length === 3);
        let [a, b, c] = r;
        assert(a === 3);
        assert(b === 0);
        assert(c === 3);
    }
}

function test4() {
    function baz(...args) {
        return args;
    }
    function bar(...args) {
        return baz(...args);
    }
    function foo() {
        let args = [];
        return bar(...args, 0, ...args);
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++) {
        let r = foo();
        assert(r.length === 1);
        assert(r[0] === 0);
    }
}

test1();
test2();
test3();
test4();
