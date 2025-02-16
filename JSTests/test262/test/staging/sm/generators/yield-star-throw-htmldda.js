// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
features:
  - IsHTMLDDA
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-generators-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
function* g(iter) {
    yield* iter;
}

var calledReturn = false;

var it = g({
    [Symbol.iterator]() {
        return this;
    },
    next() {
        return {done: false};
    },
    throw: createIsHTMLDDA(),
    return() {
        calledReturn = true;
        return {done: false};
    }
});

it.next();

assertThrowsInstanceOf(() => it.throw(""), TypeError);

assert.sameValue(calledReturn, false);

