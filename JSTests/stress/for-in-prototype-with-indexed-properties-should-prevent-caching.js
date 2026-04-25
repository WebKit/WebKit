"use strict";

function assert(b) {
    if (!b)
        throw new Error;
}


function test1() {
    function foo(o) {
        let result = [];
        for (let p in o)
            result.push(p);
        return result;
    }
    noInline(foo);

    let p = {};
    let x = {__proto__: p};
    p[0] = 25;
    for (let i = 0; i < 20; ++i) {
        let result = foo(x);
        assert(result.length === 1);
        assert(result[0] === "0");
    }

    p[1] = 30;
    for (let i = 0; i < 20; ++i) {
        let result = foo(x);
        assert(result.length === 2);
        assert(result[0] === "0");
        assert(result[1] === "1");
    }

    p[2] = {};
    for (let i = 0; i < 20; ++i) {
        let result = foo(x);
        assert(result.length === 3);
        assert(result[0] === "0");
        assert(result[1] === "1");
        assert(result[2] === "2");
    }
}
test1();

function test2() {
    function foo(o) {
        let result = [];
        for (let p in o)
            result.push(p);
        return result;
    }
    noInline(foo);

    let p = {};
    let x = {__proto__: p};
    for (let i = 0; i < 20; ++i) {
        let result = foo(x);
        assert(result.length === 0);
    }

    p[0] = 30;
    for (let i = 0; i < 20; ++i) {
        let result = foo(x);
        assert(result.length === 1);
        assert(result[0] === "0");
    }

    p[1] = {};
    for (let i = 0; i < 20; ++i) {
        let result = foo(x);
        assert(result.length === 2);
        assert(result[0] === "0");
        assert(result[1] === "1");
    }
}
test2();
