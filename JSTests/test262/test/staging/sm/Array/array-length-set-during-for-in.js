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
var a = [0, 1];
var iterations = 0;
for (var k in a) {
  iterations++;
  a.length = 1;
}
assert.sameValue(iterations, 1);

