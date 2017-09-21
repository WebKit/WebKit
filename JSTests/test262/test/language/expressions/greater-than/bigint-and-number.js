// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Comparisons of BigInt and Number values
esid: sec-abstract-relational-comparison
info: |
  ...
  3. If both px and py are Strings, then
    ...
  4. Else,
    a. Let nx be ? ToNumeric(px). Because px and py are primitive values evaluation order is not important.
    b. Let ny be ? ToNumeric(py).
    c. If Type(nx) is Type(ny), return ? Type(nx)::lessThan(nx, ny).
    d. Assert: Type(nx) is BigInt and Type(ny) is Number, or if Type(nx) is Number and Type(ny) is BigInt.
    e. If x or y are any of NaN, return undefined.
    f. If x is -∞, or y is +∞, return true.
    g. If x is +∞, or y is -∞, return false.
    h. If the mathematical value of nx is less than the mathematical value of ny, return true, otherwise return false.
features: [BigInt]
---*/

assert.sameValue(1n > 0, true);
assert.sameValue(1n > 0.999999999999, true);
assert.sameValue(1 > 0n, true);
assert.sameValue(0.000000000001 > 0n, true);
assert.sameValue(1n > 1, false);
assert.sameValue(1 > 1n, false);
assert.sameValue(0n > 1, false);
assert.sameValue(0 > 1n, false);
assert.sameValue(0 > 0n, false);
assert.sameValue(0n > 0, false);
assert.sameValue(1n > Number.MAX_VALUE, false);
assert.sameValue(Number.MIN_VALUE > 0n, true);
assert.sameValue(-10n > Number.MIN_VALUE, false);
assert.sameValue(Number.MAX_VALUE > 10000000000n, true);
