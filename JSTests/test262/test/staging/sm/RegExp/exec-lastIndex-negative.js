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
var BUGNUMBER = 1207922;
var summary = "negative lastIndex should be treated as 0.";

print(BUGNUMBER + ": " + summary);

var pattern = /abc/gi;
var string = 'AbcaBcabC';

var indices = [
    -1,
    -Math.pow(2,32),
    -(Math.pow(2,32) + 1),
    -Math.pow(2,32) * 2,
    -Math.pow(2,40),
    -Number.MAX_VALUE,
];
for (var index of indices) {
  pattern.lastIndex = index;
  var result = pattern.exec(string);
  assert.sameValue(result.index, 0);
  assert.sameValue(result.length, 1);
  assert.sameValue(result[0], "Abc");
  assert.sameValue(pattern.lastIndex, 3);
}

