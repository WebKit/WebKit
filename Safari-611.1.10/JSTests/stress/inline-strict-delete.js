//@ requireOptions("--useConcurrentJIT=0")

function assert(b) {
    if (!b)
        throw new Error;
}
function bar(o) {
    "use strict";
    delete o.p;
}
function foo(b, o) {
    let x = {};
    if (b)
        Object.defineProperty(x, "p", {value: 42, configurable: false});
    let threw = false;
    try {
        bar(x);
    } catch {
        threw = true;
    }
    if (b)
        assert(threw);
    else
        assert(!threw);
}
noInline(foo);
for (let i = 0; i < 1000; ++i) foo(!!(i % 2));
