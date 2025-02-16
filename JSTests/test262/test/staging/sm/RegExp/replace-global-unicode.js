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
var BUGNUMBER = 1287524;
var summary = 'RegExp.prototype[@@replace] should not use optimized path if RegExp.prototype.unicode is modified.';

print(BUGNUMBER + ": " + summary);

Object.defineProperty(RegExp.prototype, "unicode", {
  get() {
    RegExp.prototype.exec = () => null;
  }
});

var rx = RegExp("a", "g");
var s = "abba";
var r = rx[Symbol.replace](s, "c");
assert.sameValue(r, "abba");

