// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-String-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 887016;
var summary = "Call RegExp.prototype[@@split] from String.prototype.split.";

print(BUGNUMBER + ": " + summary);

var called = 0;
var myRegExp = {
  [Symbol.split](S, limit) {
    assert.sameValue(S, "abcAbcABC");
    assert.sameValue(limit, 10);
    called++;
    return ["X", "Y", "Z"];
  }
};
assert.sameValue("abcAbcABC".split(myRegExp, 10).join(","), "X,Y,Z");
assert.sameValue(called, 1);

