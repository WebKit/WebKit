// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-object-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var obj = { method() { } };
assertThrowsInstanceOf(() => {
    new obj.method;
}, TypeError);

obj = { constructor() { } };
assertThrowsInstanceOf(() => {
    new obj.constructor;
}, TypeError);

