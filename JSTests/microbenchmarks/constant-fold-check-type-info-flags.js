"use strict";

function clobber() { }
noInline(clobber);

class C { }
class D { }

function foo(x, C) {
    clobber();
    return x instanceof C;
}
noInline(foo);

function access(o) {
    return o.foo0;
}
noInline(access);

function theClass(i) {
    if (i & 1)
        return C;
    return D;
}
noInline(theClass);

let x = new C;
for (let i = 0; i < 1000; ++i) {
    let k = theClass(i);
    if (i < 20)
        k["foo" + i] = i;
    if (i >= 20)
        access(k);
    if (i === 100)
        k["foo" + i] = i;
    let result = foo(x, k);
    if (k === C && result !== true)
        throw new Error("Bad")
    if (k !== C && result !== false)
        throw new Error("Bad")
}
