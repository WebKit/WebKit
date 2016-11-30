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
    function foo(...args) {
        return bar(...args);
    }
    noInline(foo);

    for (let i = 0; i < 10000; i++) {
        let [a, b, c, d] = foo(i, i+1, i+2, i+3);
        assert(a === i);
        assert(b === i+1);
        assert(c === i+2);
        assert(d === i+3) ;
    }
}

function test2() {
    function bar(...args) {
        return args;
    }
    function foo(a, ...args) {
        return bar(...args, a, ...args);
    }
    noInline(foo);

    for (let i = 0; i < 10000; i++) {
        let r = foo(i, i+1, i+2, i+3);
        assert(r.length === 7);
        let [a, b, c, d, e, f, g] = r;
        assert(a === i+1);
        assert(b === i+2);
        assert(c === i+3);
        assert(d === i);
        assert(e === i+1);
        assert(f === i+2);
        assert(g === i+3);
    }
}

function test3() {
    function baz(...args) {
        return args;
    }
    function bar(...args) {
        return baz(...args);
    }
    function foo(a, b, c, ...args) {
        return bar(...args, a, ...args);
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++) {
        let r = foo(i, i+1, i+2, i+3);
        assert(r.length === 3);
        let [a, b, c] = r;
        assert(a === i+3);
        assert(b === i);
        assert(c === i+3);
    }
}

function test4() {
    function baz(...args) {
        return args;
    }
    function bar(...args) {
        return baz(...args);
    }
    function foo(a, b, c, d, ...args) {
        return bar(...args, a, ...args);
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++) {
        let r = foo(i, i+1, i+2, i+3);
        assert(r.length === 1);
        assert(r[0] === i);
    }
}

function test5() {
    function baz(a, b, c) {
        return [a, b, c];
    }
    function bar(...args) {
        return baz(...args);
    }
    function foo(a, b, c, d, ...args) {
        return bar(...args, a, ...args);
    }
    noInline(foo);

    for (let i = 0; i < 100000; i++) {
        let r = foo(i, i+1, i+2, i+3);
        assert(r.length === 3);
        let [a, b, c] = r;
        assert(a === i);
        assert(b === undefined);
        assert(c === undefined);
    }
}

test1();
test2();
test3();
test4();
test5();
