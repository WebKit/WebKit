// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.duration
description: Various arguments to the Duration constructor that are out of range
features: [Temporal]
---*/

// 2^32 = 4294967296
assert.throws(RangeError, () => new Temporal.Duration(4294967296), "years > max");
assert.throws(RangeError, () => new Temporal.Duration(-4294967296), "years < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 4294967296), "months > max");
assert.throws(RangeError, () => new Temporal.Duration(0, -4294967296), "months < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 4294967296), "weeks > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, -4294967296), "weeks < min");

// ceil(max safe integer / 86400) = 104249991375
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 104249991375), "days > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 104249991374, 24), "hours balance into days > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, -104249991375), "days < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, -104249991374, -24), "hours balance into days < min");

// ceil(max safe integer / 3600) = 2501999792984
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 2501999792984), "hours > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 2501999792983, 60), "minutes balance into hours > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, -2501999792984), "hours < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, -2501999792983, -60), "minutes balance into hours < min");

// ceil(max safe integer / 60) = 150119987579017
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 150119987579017), "minutes > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 150119987579016, 60), "seconds balance into minutes > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, -150119987579017), "minutes < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, -150119987579016, -60), "seconds balance into minutes < min");

// 2^53 = 9007199254740992
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, 9007199254740992), "seconds > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, 9007199254740991, 1000), "ms balance into seconds > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, 9007199254740991, 999, 1000), "µs balance into seconds > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, 9007199254740991, 999, 999, 1000), "ns balance into seconds > max");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, -9007199254740992), "seconds < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, -9007199254740991, -1000), "ms balance into seconds < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, -9007199254740991, -999, -1000), "µs balance into seconds < min");
assert.throws(RangeError, () => new Temporal.Duration(0, 0, 0, 0, 0, 0, -9007199254740991, -999, -999, -1000), "ns balance into seconds < min");
