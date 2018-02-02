// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Non-strict inequality comparison of BigInt and Number values
esid: sec-abstract-equality-comparison
info: |
  12. If Type(x) is BigInt and Type(y) is Number, or if Type(x) is Number and Type(y) is BigInt,
    b. If the mathematical value of x is equal to the mathematical value of y, return true, otherwise return false.

features: [BigInt]
---*/

assert.sameValue(0n != 0, false, "0n != 0");
assert.sameValue(0 != 0n, false, "0 != 0n");
assert.sameValue(0n != -0, false, "0n != -0");
assert.sameValue(-0 != 0n, false, "-0 != 0n");
assert.sameValue(0n != 0.000000000001, true, "0n != 0.000000000001");
assert.sameValue(0.000000000001 != 0n, true, "0.000000000001 != 0n");
assert.sameValue(0n != 1, true, "0n != 1");
assert.sameValue(1 != 0n, true, "1 != 0n");
assert.sameValue(1n != 0, true, "1n != 0");
assert.sameValue(0 != 1n, true, "0 != 1n");
assert.sameValue(1n != 0.999999999999, true, "1n != 0.999999999999");
assert.sameValue(0.999999999999 != 1n, true, "0.999999999999 != 1n");
assert.sameValue(1n != 1, false, "1n != 1");
assert.sameValue(1 != 1n, false, "1 != 1n");
assert.sameValue(0n != Number.MIN_VALUE, true, "0n != Number.MIN_VALUE");
assert.sameValue(Number.MIN_VALUE != 0n, true, "Number.MIN_VALUE != 0n");
assert.sameValue(0n != -Number.MIN_VALUE, true, "0n != -Number.MIN_VALUE");
assert.sameValue(-Number.MIN_VALUE != 0n, true, "-Number.MIN_VALUE != 0n");
assert.sameValue(-10n != Number.MIN_VALUE, true, "-10n != Number.MIN_VALUE");
assert.sameValue(Number.MIN_VALUE != -10n, true, "Number.MIN_VALUE != -10n");
