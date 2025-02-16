// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-generators-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var BUGNUMBER = 1099956;
var summary =
  "The token next to yield should be tokenized as non-operand if yield is " +
  "a valid name";

printBugNumber(BUGNUMBER);
printStatus(summary);

var yield = 12, a = 3, b = 6, g = 2;
var yieldParsedAsIdentifier = false;

yield /a; yieldParsedAsIdentifier = true; b/g;

assert.sameValue(yieldParsedAsIdentifier, true);

