// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var getProtoCalled = false;

var newTarget = Object.defineProperty(function(){}.bind(), "prototype", {
    get() {
        getProtoCalled = true;
        return null;
    }
});

var AsyncFunction = async function(){}.constructor;

assertThrowsInstanceOf(() => {
    Reflect.construct(AsyncFunction, ["@error"], newTarget);
}, SyntaxError);

assert.sameValue(getProtoCalled, false);

