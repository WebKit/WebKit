// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Comparisons of BigInt and BigInt values
esid: sec-abstract-relational-comparison
info: |
  ...
  3. If both px and py are Strings, then
    ...
  4. Else,
    a. Let nx be ? ToNumeric(px). Because px and py are primitive values evaluation order is not important.
    b. Let ny be ? ToNumeric(py).
    c. If Type(nx) is Type(ny), return ? Type(nx)::lessThan(nx, ny).

  sec-numeric-types-bigint-lessThan
  BigInt::lessThan (x, y)

    The abstract operation BigInt::lessThan with two arguments x and y of BigInt type returns true if x is less than y and false otherwise.

features: [BigInt]
---*/

assert.sameValue(0n > 0n, false, "0n > 0n");
assert.sameValue(1n > 1n, false, "1n > 1n");
assert.sameValue(-1n > -1n, false, "-1n > -1n");
assert.sameValue(0n > -0n, false, "0n > -0n");
assert.sameValue(-0n > 0n, false, "-0n > 0n");
assert.sameValue(0n > 1n, false, "0n > 1n");
assert.sameValue(1n > 0n, true, "1n > 0n");
assert.sameValue(0n > -1n, true, "0n > -1n");
assert.sameValue(-1n > 0n, false, "-1n > 0n");
assert.sameValue(1n > -1n, true, "1n > -1n");
assert.sameValue(-1n > 1n, false, "-1n > 1n");
assert.sameValue(0x1fffffffffffff01n > 0x1fffffffffffff02n, false, "0x1fffffffffffff01n > 0x1fffffffffffff02n");
assert.sameValue(0x1fffffffffffff02n > 0x1fffffffffffff01n, true, "0x1fffffffffffff02n > 0x1fffffffffffff01n");
assert.sameValue(-0x1fffffffffffff01n > -0x1fffffffffffff02n, true, "-0x1fffffffffffff01n > -0x1fffffffffffff02n");
assert.sameValue(-0x1fffffffffffff02n > -0x1fffffffffffff01n, false, "-0x1fffffffffffff02n > -0x1fffffffffffff01n");
assert.sameValue(0x10000000000000000n > 0n, true, "0x10000000000000000n > 0n");
assert.sameValue(0n > 0x10000000000000000n, false, "0n > 0x10000000000000000n");
assert.sameValue(0x10000000000000000n > 1n, true, "0x10000000000000000n > 1n");
assert.sameValue(1n > 0x10000000000000000n, false, "1n > 0x10000000000000000n");
assert.sameValue(0x10000000000000000n > -1n, true, "0x10000000000000000n > -1n");
assert.sameValue(-1n > 0x10000000000000000n, false, "-1n > 0x10000000000000000n");
assert.sameValue(0x10000000000000001n > 0n, true, "0x10000000000000001n > 0n");
assert.sameValue(0n > 0x10000000000000001n, false, "0n > 0x10000000000000001n");
assert.sameValue(-0x10000000000000000n > 0n, false, "-0x10000000000000000n > 0n");
assert.sameValue(0n > -0x10000000000000000n, true, "0n > -0x10000000000000000n");
assert.sameValue(-0x10000000000000000n > 1n, false, "-0x10000000000000000n > 1n");
assert.sameValue(1n > -0x10000000000000000n, true, "1n > -0x10000000000000000n");
assert.sameValue(-0x10000000000000000n > -1n, false, "-0x10000000000000000n > -1n");
assert.sameValue(-1n > -0x10000000000000000n, true, "-1n > -0x10000000000000000n");
assert.sameValue(-0x10000000000000001n > 0n, false, "-0x10000000000000001n > 0n");
assert.sameValue(0n > -0x10000000000000001n, true, "0n > -0x10000000000000001n");
assert.sameValue(0x10000000000000000n > 0x100000000n, true, "0x10000000000000000n > 0x100000000n");
assert.sameValue(0x100000000n > 0x10000000000000000n, false, "0x100000000n > 0x10000000000000000n");
