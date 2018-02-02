// Copyright (C) 2017 Josh Wolfe. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Non-strict equality comparison of BigInt and String values
esid: sec-abstract-equality-comparison
info: |
  1. If Type(x) is different from Type(y), return false.

features: [BigInt]
---*/

assert.sameValue(0n == "", true, '0n == ""');
assert.sameValue("" == 0n, true, '"" == 0n');
assert.sameValue(0n == "-0", true, '0n == "-0"');
assert.sameValue("-0" == 0n, true, '"-0" == 0n');
assert.sameValue(0n == "0", true, '0n == "0"');
assert.sameValue("0" == 0n, true, '"0" == 0n');
assert.sameValue(0n == "-1", false, '0n == "-1"');
assert.sameValue("-1" == 0n, false, '"-1" == 0n');
assert.sameValue(0n == "1", false, '0n == "1"');
assert.sameValue("1" == 0n, false, '"1" == 0n');
assert.sameValue(0n == "foo", false, '0n == "foo"');
assert.sameValue("foo" == 0n, false, '"foo" == 0n');
assert.sameValue(1n == "", false, '1n == ""');
assert.sameValue("" == 1n, false, '"" == 1n');
assert.sameValue(1n == "-0", false, '1n == "-0"');
assert.sameValue("-0" == 1n, false, '"-0" == 1n');
assert.sameValue(1n == "0", false, '1n == "0"');
assert.sameValue("0" == 1n, false, '"0" == 1n');
assert.sameValue(1n == "-1", false, '1n == "-1"');
assert.sameValue("-1" == 1n, false, '"-1" == 1n');
assert.sameValue(1n == "1", true, '1n == "1"');
assert.sameValue("1" == 1n, true, '"1" == 1n');
assert.sameValue(1n == "foo", false, '1n == "foo"');
assert.sameValue("foo" == 1n, false, '"foo" == 1n');
assert.sameValue(-1n == "-", false, '-1n == "-"');
assert.sameValue("-" == -1n, false, '"-" == -1n');
assert.sameValue(-1n == "-0", false, '-1n == "-0"');
assert.sameValue("-0" == -1n, false, '"-0" == -1n');
assert.sameValue(-1n == "-1", true, '-1n == "-1"');
assert.sameValue("-1" == -1n, true, '"-1" == -1n');
assert.sameValue(-1n == "-foo", false, '-1n == "-foo"');
assert.sameValue("-foo" == -1n, false, '"-foo" == -1n');
assert.sameValue(900719925474099101n == "900719925474099101", true, '900719925474099101n == "900719925474099101"');
assert.sameValue("900719925474099101" == 900719925474099101n, true, '"900719925474099101" == 900719925474099101n');
assert.sameValue(900719925474099102n == "900719925474099101", false, '900719925474099102n == "900719925474099101"');
assert.sameValue("900719925474099101" == 900719925474099102n, false, '"900719925474099101" == 900719925474099102n');
