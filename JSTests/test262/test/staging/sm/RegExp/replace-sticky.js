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
var summary = "String.prototype.replace should use and update lastIndex if sticky flag is set";

print(BUGNUMBER + ": " + summary);

var input = "abcdeabcdeabcdefghij";
var re = new RegExp("abcde", "y");
re.test(input);
assert.sameValue(re.lastIndex, 5);
var ret = input.replace(re, "ABCDE");
assert.sameValue(ret, "abcdeABCDEabcdefghij");
assert.sameValue(re.lastIndex, 10);
ret = input.replace(re, "ABCDE");
assert.sameValue(ret, "abcdeabcdeABCDEfghij");
assert.sameValue(re.lastIndex, 15);
ret = input.replace(re, "ABCDE");
assert.sameValue(ret, "abcdeabcdeabcdefghij");
assert.sameValue(re.lastIndex, 0);

