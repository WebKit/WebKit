// Copyright (C) 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Relational comparison of BigInt and string values
esid: sec-abstract-relational-comparison
features: [BigInt]
---*/

assert.sameValue("0n" < 1n, false, "'0n' < 1n");
assert.sameValue("0." < 1n, false, "'0.' < 1n");
assert.sameValue(".0" < 1n, false, "'.0' < 1n");
assert.sameValue("0/1" < 1n, false, "'0/1' < 1n");
assert.sameValue("z0" < 1n, false, "'z0' < 1n");
assert.sameValue("0z" < 1n, false, "'0z' < 1n");
assert.sameValue("++0" < 1n, false, "'++0' < 1n");
assert.sameValue("--0" < 1n, false, "'--0' < 1n");
assert.sameValue("0e0" < 1n, false, "'0e0' < 1n");
assert.sameValue("Infinity" < 1n, false, "'Infinity' < 1n");

assert.sameValue(0n < "1n", false, "0n < '1n'");
assert.sameValue(0n < "1.", false, "0n < '1.'");
assert.sameValue(0n < ".1", false, "0n < '.1'");
assert.sameValue(0n < "1/1", false, "0n < '1/1'");
assert.sameValue(0n < "z1", false, "0n < 'z1'");
assert.sameValue(0n < "1z", false, "0n < '1z'");
assert.sameValue(0n < "++1", false, "0n < '++1'");
assert.sameValue(0n < "--1", false, "0n < '--1'");
assert.sameValue(0n < "1e0", false, "0n < '1e0'");
assert.sameValue(0n < "Infinity", false, "0n < 'Infinity'");
