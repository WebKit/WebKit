// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 887016;
var summary = "RegExp.prototype[@@split] should check if this value is RegExp.";

print(BUGNUMBER + ": " + summary);

var obj = { flags: "", toString: () => "-" };
assert.deepEqual(RegExp.prototype[Symbol.split].call(obj, "a-b-c"),
             ["a", "b", "c"]);

