"use strict";

// This is our nifty way to make modules synchronous.
let assert;
import('./assert.js').then((module) => {
    assert = module;
}, $vm.crash);
drainMicrotasks();

function test(func, description) {
    try {
        func();
    } catch (e) {
        print("Unexpected exception:", description);
        throw e;
    }
}

function promise_test(func, description) {
    assert.asyncTest(func());
};

let assert_equals = assert.eq;
let assert_true = (x) => assert.eq(x,true);
let assert_false = (x) => assert.eq(x,false);
let assert_unreached = () => {
    throw new Error("Should have been unreachable");
};

let console = {
    log(...args) {
        print(...args);
    }
};

load("./tail-call-spec-harness/sync_index.js", "caller relative");
load("./tail-call-spec-harness/wasm-constants.js", "caller relative");
load("./tail-call-spec-harness/wasm-module-builder.js", "caller relative");
