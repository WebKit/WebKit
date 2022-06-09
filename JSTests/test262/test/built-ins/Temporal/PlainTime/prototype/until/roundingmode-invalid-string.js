// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.until
description: RangeError thrown when roundingMode option not one of the allowed string values
features: [Temporal]
---*/

const earlier = new Temporal.PlainTime(12, 34, 56, 0, 0, 0);
const later = new Temporal.PlainTime(13, 35, 57, 123, 987, 500);
assert.throws(RangeError, () => earlier.until(later, { smallestUnit: "microsecond", roundingMode: "other string" }));
