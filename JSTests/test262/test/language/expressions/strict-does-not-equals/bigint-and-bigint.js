// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Strict inequality comparison of BigInt values
esid: sec-strict-equality-comparison
info: |
  1. If Type(x) is different from Type(y), return false.
  2. If Type(x) is Number or BigInt, then
    a. Return ! Type(x)::equal(x, y).

  sec-numeric-types-bigint-equal
  BigInt::equal (x, y)

    The abstract operation BigInt::equal with two arguments x and y of BigInt type returns true if x and y have the same mathematical integer value and false otherwise.

features: [BigInt]
---*/

assert.sameValue(0n !== 0n, false, "0n !== 0n");
assert.sameValue(1n !== 1n, false, "1n !== 1n");
assert.sameValue(-1n !== -1n, false, "-1n !== -1n");
assert.sameValue(0n !== -0n, false, "0n !== -0n");
assert.sameValue(-0n !== 0n, false, "-0n !== 0n");
assert.sameValue(0n !== 1n, true, "0n !== 1n");
assert.sameValue(1n !== 0n, true, "1n !== 0n");
assert.sameValue(0n !== -1n, true, "0n !== -1n");
assert.sameValue(-1n !== 0n, true, "-1n !== 0n");
assert.sameValue(1n !== -1n, true, "1n !== -1n");
assert.sameValue(-1n !== 1n, true, "-1n !== 1n");
assert.sameValue(0x1fffffffffffff01n !== 0x1fffffffffffff01n, false, "0x1fffffffffffff01n !== 0x1fffffffffffff01n");
assert.sameValue(0x1fffffffffffff01n !== 0x1fffffffffffff02n, true, "0x1fffffffffffff01n !== 0x1fffffffffffff02n");
assert.sameValue(0x1fffffffffffff02n !== 0x1fffffffffffff01n, true, "0x1fffffffffffff02n !== 0x1fffffffffffff01n");
assert.sameValue(-0x1fffffffffffff01n !== -0x1fffffffffffff01n, false, "-0x1fffffffffffff01n !== -0x1fffffffffffff01n");
assert.sameValue(-0x1fffffffffffff01n !== -0x1fffffffffffff02n, true, "-0x1fffffffffffff01n !== -0x1fffffffffffff02n");
assert.sameValue(-0x1fffffffffffff02n !== -0x1fffffffffffff01n, true, "-0x1fffffffffffff02n !== -0x1fffffffffffff01n");
assert.sameValue(0x10000000000000000n !== 0n, true, "0x10000000000000000n !== 0n");
assert.sameValue(0n !== 0x10000000000000000n, true, "0n !== 0x10000000000000000n");
assert.sameValue(0x10000000000000000n !== 1n, true, "0x10000000000000000n !== 1n");
assert.sameValue(1n !== 0x10000000000000000n, true, "1n !== 0x10000000000000000n");
assert.sameValue(0x10000000000000000n !== -1n, true, "0x10000000000000000n !== -1n");
assert.sameValue(-1n !== 0x10000000000000000n, true, "-1n !== 0x10000000000000000n");
assert.sameValue(0x10000000000000001n !== 0n, true, "0x10000000000000001n !== 0n");
assert.sameValue(0n !== 0x10000000000000001n, true, "0n !== 0x10000000000000001n");
assert.sameValue(-0x10000000000000000n !== 0n, true, "-0x10000000000000000n !== 0n");
assert.sameValue(0n !== -0x10000000000000000n, true, "0n !== -0x10000000000000000n");
assert.sameValue(-0x10000000000000000n !== 1n, true, "-0x10000000000000000n !== 1n");
assert.sameValue(1n !== -0x10000000000000000n, true, "1n !== -0x10000000000000000n");
assert.sameValue(-0x10000000000000000n !== -1n, true, "-0x10000000000000000n !== -1n");
assert.sameValue(-1n !== -0x10000000000000000n, true, "-1n !== -0x10000000000000000n");
assert.sameValue(-0x10000000000000001n !== 0n, true, "-0x10000000000000001n !== 0n");
assert.sameValue(0n !== -0x10000000000000001n, true, "0n !== -0x10000000000000001n");
assert.sameValue(0x10000000000000000n !== 0x100000000n, true, "0x10000000000000000n !== 0x100000000n");
assert.sameValue(0x100000000n !== 0x10000000000000000n, true, "0x100000000n !== 0x10000000000000000n");
