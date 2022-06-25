// Copyright (c) 2016 The V8 Project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-number.issafeinteger
description: >
  Return false if argument is not a safe integer
info: |
  Number.isSafeInteger ( number )

  [...]
  3. Let integer be ToInteger(number).
  4. If integer is not equal to number, return false.
  5. If abs(integer) ≤ 2**53-1, return true.
  6. Otherwise, return false.
---*/

assert.sameValue(Number.isSafeInteger(9007199254740992), false, "2**53");
assert.sameValue(Number.isSafeInteger(-9007199254740992), false, "-(2**53)");
