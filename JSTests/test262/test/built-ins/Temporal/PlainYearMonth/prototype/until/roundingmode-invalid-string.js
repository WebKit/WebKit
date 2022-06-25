// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.until
description: RangeError thrown when roundingMode option not one of the allowed string values
features: [Temporal]
---*/

const earlier = new Temporal.PlainYearMonth(2000, 5);
const later = new Temporal.PlainYearMonth(2001, 6);
for (const roundingMode of ["other string", "cile", "CEIL", "ce\u0131l", "auto", "expand", "halfCeil", "halfFloor", "halfTrunc", "halfEven", "halfexpand", "floor\0"]) {
  assert.throws(RangeError, () => earlier.until(later, { smallestUnit: "microsecond", roundingMode }));
}
