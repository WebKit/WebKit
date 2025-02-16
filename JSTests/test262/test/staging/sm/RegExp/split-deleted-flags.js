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
var BUGNUMBER = 1322319;
var summary = "RegExp.prototype.split should throw if RegRxp.prototype.flags is deleted."

print(BUGNUMBER + ": " + summary);

delete RegExp.prototype.flags;

assertThrowsInstanceOf(() => "aaaaa".split(/a/), SyntaxError);

