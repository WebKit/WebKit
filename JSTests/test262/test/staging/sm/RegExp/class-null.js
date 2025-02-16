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
var BUGNUMBER = 1279467;
var summary = "Null in character class in RegExp with unicode flag.";

print(BUGNUMBER + ": " + summary);

var m = /([\0]+)/u.exec("\u0000");
assert.sameValue(m.length, 2);
assert.sameValue(m[0], '\u0000');
assert.sameValue(m[1], '\u0000');

var m = /([\0]+)/u.exec("0");
assert.sameValue(m, null);

