// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tostring
description: RangeError thrown when smallestUnit option not one of the allowed string values
features: [Temporal]
---*/

const time = new Temporal.PlainTime(12, 34, 56, 123, 987, 500);
for (const smallestUnit of ["era", "year", "month", "day", "hour", "nonsense", "other string", "m\u0131nute", "SECOND"]) {
  assert.throws(RangeError, () => time.toString({ smallestUnit }));
}
