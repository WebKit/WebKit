"use strict";
function assert(cond, m) {
    if (!cond)
        throw new Error("broke assertion: '" + m + "'");
}
noInline(assert);

function baz(b) {
    if (b)
        throw new Error("Baz");
}

function bar(b) {
    var exception = null;
    try {
        baz(b);
    } catch(e) {
        exception = e;
        baz(b);
    } finally {
        if (b) 
            assert(exception);
    }
}

function foo(b) {
    var exception = null;
    try {
        bar(b);
    } catch(e) {
        exception = e;
    }
    if (b)
        assert(exception);
}

const NUM_LOOPS = 1000;
for (var i = 0; i < NUM_LOOPS; i++) {
    foo(i === NUM_LOOPS - 1);
}
