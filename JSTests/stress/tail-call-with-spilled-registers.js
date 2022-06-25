//@ requireOptions("--useConcurrentJIT=false")

"use strict";

function assert(a, e) {
  if (a !== e)
    throw new Error('Expected: ' + e + ' but got: ' + a);
}
noInline(assert);

function c3(v, b, c, d, e) {
    return v + b + c + d + e;
}
noInline(c3);

function c1(o) {
    let ret = o.c2;
    if (o.a)
      assert(o.a, 126);
    return o;
}
noInline(c1);

function getter() {
    let b = Math.random();
    let c = Math.random();
    let d = Math.random();
    let e = Math.random();
    return c3('test', b, c, d, e);
}
noInline(getter);

let c = [];

c[0] = {a: 126};
c[0].foo = 0;
c[0].c2 = 15;

c[1] = {};
c[1].bar = 99;

c[2] = {};
Object.defineProperty(c[2], 'c2', { get: getter });

for (let i = 0; i < 10000; i++) {
    if (numberOfDFGCompiles(c1) > 0)
        c1(c[2]);
    else
        c1(c[i % 2]);
}
