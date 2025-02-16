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
var BUGNUMBER = 1247701;
var summary = 'Array.prototype.shift on a dense array with holes should update for-in enumeration properties.';

print(BUGNUMBER + ": " + summary);

var x = ["a", , "b", , "c", "d" , "e", "f", "g"];
for (var p in x) {
  assert.sameValue(p in x, true);
  x.shift();
}

