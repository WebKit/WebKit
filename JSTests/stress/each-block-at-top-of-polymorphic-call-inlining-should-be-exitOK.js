"use strict";

function f() { };
noInline(f);

function foo(o, x) {
    return o.get(x);
}
noInline(foo);

let objs = [
    new Map,
    { get() { return f(); } },
];


for (let i = 0; i < 1000000; ++i) {
    foo(objs[i % objs.length], i);
}
