"use strict";

function truth() {
    return true;
}
noInline(truth);

function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);


function foo(y) {
    return y;
}

let x = 40;
assert(x === 40);

for (var i = 0; i < 1000; i++) {
    if (truth()) {
        let y = 20;
        let capY = function() { return y; }
        assert(x === 40);
        assert(capY() === 20);
        assert(foo(i) === i);
    }
}
assert(foo("hello") === "hello");
