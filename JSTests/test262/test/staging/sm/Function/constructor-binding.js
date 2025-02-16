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
var BUGNUMBER = 636635;
var summary = "A function created by Function constructor shouldn't have anonymous binding";

print(BUGNUMBER + ": " + summary);

assert.sameValue(new Function("return typeof anonymous")(), "undefined");
assert.sameValue(new Function("return function() { return typeof anonymous; }")()(), "undefined");
assert.sameValue(new Function("return function() { eval(''); return typeof anonymous; }")()(), "undefined");

