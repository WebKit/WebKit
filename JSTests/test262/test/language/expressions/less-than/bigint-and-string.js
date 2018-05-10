// Copyright (C) 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Relational comparison of BigInt and string values
esid: sec-abstract-relational-comparison
features: [BigInt]
---*/

assert.sameValue(0n < "0", false, "0n < '0'");
assert.sameValue("0" < 0n, false, "'0' < 0n");

assert.sameValue(0n < "1", true, "0n < '1'");
assert.sameValue("0" < 1n, true, "'0' < 1n");

assert.sameValue(1n < "0", false, "1n < '0'");
assert.sameValue("1" < 0n, false, "'1' < 0n");

assert.sameValue(0n < "", false, "0n < ''");
assert.sameValue("" < 0n, false, "'' < 0n");

assert.sameValue(0n < "1", true, "0n < '1'");
assert.sameValue("" < 1n, true, "'' < 1n");

assert.sameValue(1n < "", false, "1n < ''");
assert.sameValue("1" < 0n, false, "'1' < 0n");

assert.sameValue(1n < "1", false, "1n < '1'");
assert.sameValue("1" < 1n, false, "'1' < 1n");

assert.sameValue(1n < "-1", false, "1n < '-1'");
assert.sameValue("1" < -1n, false, "'1' < -1n");

assert.sameValue(-1n < "1", true, "-1n < '1'");
assert.sameValue("-1" < 1n, true, "'-1' < 1n");

assert.sameValue(-1n < "-1", false, "-1n < '-1'");
assert.sameValue("-1" < -1n, false, "'-1' < -1n");

assert.sameValue(9007199254740993n < "9007199254740992", false,
                 "9007199254740993n < '9007199254740992'");
assert.sameValue("9007199254740993" < 9007199254740992n, false,
                 "'9007199254740993' < 9007199254740992n");

assert.sameValue(-9007199254740992n < "-9007199254740993", false,
                 "-9007199254740992n < '-9007199254740993'");
assert.sameValue("-9007199254740992" < -9007199254740993n, false,
                 "'-9007199254740992' < -9007199254740993n");
