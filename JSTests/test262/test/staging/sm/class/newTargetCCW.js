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
// Make sure we wrap the new target on CCW construct calls.
var g = createNewGlobal();

let f = g.eval('(function (expected) { this.accept = new.target === expected; })');

for (let i = 0; i < 1100; i++)
    assert.sameValue(new f(f).accept, true);

