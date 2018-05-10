// Copyright (C) 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Relational comparison of BigInt and string values
esid: sec-abstract-relational-comparison
features: [BigInt]
---*/

assert.sameValue(1n > "0n", false, "1n > '0n'");
assert.sameValue(1n > "0.", false, "1n > '0.'");
assert.sameValue(1n > ".0", false, "1n > '.0'");
assert.sameValue(1n > "0/1", false, "1n > '0/1'");
assert.sameValue(1n > "z0", false, "1n > 'z0'");
assert.sameValue(1n > "0z", false, "1n > '0z'");
assert.sameValue(1n > "++0", false, "1n > '++0'");
assert.sameValue(1n > "--0", false, "1n > '--0'");
assert.sameValue(1n > "0e0", false, "1n > '0e0'");
assert.sameValue(1n > "Infinity", false, "1n > 'Infinity'");

assert.sameValue("1n" > 0n, false, "'1n' > 0n");
assert.sameValue("1." > 0n, false, "'1.' > 0n");
assert.sameValue(".1" > 0n, false, "'.1' > 0n");
assert.sameValue("1/1" > 0n, false, "'1/1' > 0n");
assert.sameValue("z1" > 0n, false, "'z1' > 0n");
assert.sameValue("1z" > 0n, false, "'1z' > 0n");
assert.sameValue("++1" > 0n, false, "'++1' > 0n");
assert.sameValue("--1" > 0n, false, "'--1' > 0n");
assert.sameValue("1e0" > 0n, false, "'1e0' > 0n");
assert.sameValue("Infinity" > 0n, false, "'Infinity' > 0n");
