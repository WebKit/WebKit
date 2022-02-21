// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.round
description: RangeError thrown when roundingMode option not one of the allowed string values
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 123, 987, 500);
assert.throws(RangeError, () => time.round({ smallestUnit: "microsecond", roundingMode: "other string" }));
