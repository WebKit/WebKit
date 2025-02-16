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
var global = createNewGlobal();
Promise.prototype.then = global.Promise.prototype.then;
p1 = new Promise(function f(r) {
    r(1);
});
p2 = p1.then(function g(){});

