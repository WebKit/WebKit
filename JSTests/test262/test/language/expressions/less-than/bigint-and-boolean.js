// Copyright (C) 2018 Caio Lima. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Relational comparison of BigInt and boolean values
esid: sec-abstract-relational-comparison
features: [BigInt]
---*/

assert.sameValue(0n < false, false, "0n < false");
assert.sameValue(false < 0n, false, "false < 0n");

assert.sameValue(0n < true, true, "0n < true");
assert.sameValue(true < 0n, false, "true < 0n");

assert.sameValue(1n < false, false, "1n < false");
assert.sameValue(false < 1n, true, "false < 1n");

assert.sameValue(1n < true, false, "1n < true");
assert.sameValue(true < 1n, false, "true < 1n");

assert.sameValue(31n < true, false, "31n < true");
assert.sameValue(true < 31n, true, "true < 31n");

assert.sameValue(-3n < true, true, "-3n < true");
assert.sameValue(true < -3n, false, "true < -3n");

assert.sameValue(-3n < false, true, "-3n < false");
assert.sameValue(false < -3n, false, "false < -3n");

