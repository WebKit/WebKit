"use strict";

class C { }
let x = new C;
C = C.bind(this);

function foo(x) {
    x.foo;
    return x instanceof C;
}
noInline(foo);

for (let i = 0; i < 1000; ++i) {
    let r = foo(x);
    if (r !== true)
        throw new Error("Bad")
}
