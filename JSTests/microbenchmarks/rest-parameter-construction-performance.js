"use strict";

function foo(...rest) {
    return rest;
}
noInline(foo);

const iters = 900000;
function test1() {
    let o = {};
    let a = [];
    for (let i = 0; i < iters; i++)
        foo(10, 20, o, 55, a, 120.341, a, o);
}

function test2() {
    function foo(...rest) { // Allow this to be inlined.
        return rest;
    }
    let o = {};
    let a = [];
    for (let i = 0; i < iters; i++)
        foo(10, 20, o, 55, a, 120.341, a, o);
}

test1();
test2();
