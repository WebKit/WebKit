// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Non-strict inequality comparison of BigInt and Boolean values
esid: sec-abstract-equality-comparison
info: |
  8. If Type(x) is Boolean, return the result of the comparison ToNumber(x) == y.
  9. If Type(y) is Boolean, return the result of the comparison x == ToNumber(y).
  ...
  12. If Type(x) is BigInt and Type(y) is Number, or if Type(x) is Number and Type(y) is BigInt,
    ...
    b. If the mathematical value of x is equal to the mathematical value of y, return true, otherwise return false.

features: [BigInt]
---*/

assert.sameValue(-1n != false, true, "-1n != false");
assert.sameValue(false != -1n, true, "false != -1n");
assert.sameValue(-1n != true, true, "-1n != true");
assert.sameValue(true != -1n, true, "true != -1n");
assert.sameValue(0n != false, false, "0n != false");
assert.sameValue(false != 0n, false, "false != 0n");
assert.sameValue(0n != true, true, "0n != true");
assert.sameValue(true != 0n, true, "true != 0n");
assert.sameValue(1n != false, true, "1n != false");
assert.sameValue(false != 1n, true, "false != 1n");
assert.sameValue(1n != true, false, "1n != true");
assert.sameValue(true != 1n, false, "true != 1n");
assert.sameValue(2n != false, true, "2n != false");
assert.sameValue(false != 2n, true, "false != 2n");
assert.sameValue(2n != true, true, "2n != true");
assert.sameValue(true != 2n, true, "true != 2n");
