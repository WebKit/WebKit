//@ runDefault
// This test should not crash.

function foo() {
    "use strict";
    return --arguments["callee"];
};

function test() {
    for (var i = 0; i < 10000; i++) {
        try {
            foo();
        } catch(e) {
        }
    }
}

test();
