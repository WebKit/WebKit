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
var BUGNUMBER = 1287525;
var summary = "RegExp.prototype[@@split] shouldn't use optimized path if limit is not number.";

print(BUGNUMBER + ": " + summary);

var rx = /a/;
var r = rx[Symbol.split]("abba", {valueOf() {
  RegExp.prototype.exec = () => null;
  return 100;
}});
assert.sameValue(r.length, 1);

