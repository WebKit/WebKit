"use strict";

// This test passes when JSC doesn't crash.

let p = new Proxy(function() { }, {
    apply: function() {
        return bar();
    }
});

function bar() {
    let item = getItem();
    return item.foo;
}

let i;
let shouldReturnBad = false;
let good = [function() {return 1}, {b: 20}, {c: 40}, {d:50}]
let bad = [{asdfhasf: 20}, {e:50}, {j:70}, {k:100}, null];
function getItem() {
    if (shouldReturnBad)
        return bad[i % bad.length];
    return good[i % good.length];
}
noInline(getItem);

function start() {
    for (i = 0; i < 1000; i++) {
        p();
    }

    shouldReturnBad = true;
    for (i = 0; i < 10000; i++) {
        try {
            p();
        } catch(e) { }
    }
}
start();
