// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-RegExp-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 887016;
var summary = "String.prototype.replace should do nothing if lastIndex is invalid for sticky RegExp";

print(BUGNUMBER + ": " + summary);

var re = /a/y;
re.lastIndex = -1;
assert.sameValue("a".replace(re, "b"), "b");
re.lastIndex = 0;
assert.sameValue("a".replace(re, "b"), "b");
re.lastIndex = 1;
assert.sameValue("a".replace(re, "b"), "a");
re.lastIndex = 2;
assert.sameValue("a".replace(re, "b"), "a");
re.lastIndex = "foo";
assert.sameValue("a".replace(re, "b"), "b");
re.lastIndex = "1";
assert.sameValue("a".replace(re, "b"), "a");
re.lastIndex = {};
assert.sameValue("a".replace(re, "b"), "b");

