"use strict";

// This is our nifty way to make modules synchronous.
let assert;
import('assert.js').then((module) => {
    assert = module;
});
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

// This is run from the spec-tests directory
load("../spec-harness/index.js");
load("../spec-harness/wasm-constants.js");
load("../spec-harness/wasm-module-builder.js");
