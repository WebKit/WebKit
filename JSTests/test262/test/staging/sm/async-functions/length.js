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
var BUGNUMBER = 1185106;
var summary = "async function length";

print(BUGNUMBER + ": " + summary);

assert.sameValue(async function() {}.length, 0);
assert.sameValue(async function(a) {}.length, 1);
assert.sameValue(async function(a, b, c) {}.length, 3);
assert.sameValue(async function(a, b, c, ...d) {}.length, 3);

