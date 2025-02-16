// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1263139;
var summary = "String.prototype.match with non-string non-standard flags argument.";

print(BUGNUMBER + ": " + summary);

var called;
var flags = {
  toString() {
    called = true;
    return "";
  }
};

called = false;
"a".match("a", flags);
assert.sameValue(called, false);

called = false;
"a".search("a", flags);
assert.sameValue(called, false);

called = false;
"a".replace("a", "b", flags);
assert.sameValue(called, false);

