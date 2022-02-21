// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tostring
description: ceil value for roundingMode option
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 123, 987, 500);

const result1 = time.toString({ smallestUnit: "microsecond", roundingMode: "ceil" });
assert.sameValue(result1, "12:34:56.123988", "roundingMode is ceil");

const result2 = time.toString({ smallestUnit: "millisecond", roundingMode: "ceil" });
assert.sameValue(result2, "12:34:56.124", "roundingMode is ceil");

const result3 = time.toString({ smallestUnit: "second", roundingMode: "ceil" });
assert.sameValue(result3, "12:34:57", "roundingMode is ceil");
