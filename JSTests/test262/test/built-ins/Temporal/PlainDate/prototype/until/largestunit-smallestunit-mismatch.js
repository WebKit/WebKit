// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
description: RangeError thrown when smallestUnit is larger than largestUnit
features: [Temporal]
---*/

const earlier = Temporal.PlainDate.from("2019-01-08");
const later = Temporal.PlainDate.from("2021-09-07");
const units = ["years", "months", "weeks", "days"];
for (let largestIdx = 1; largestIdx < units.length; largestIdx++) {
  for (let smallestIdx = 0; smallestIdx < largestIdx; smallestIdx++) {
    const largestUnit = units[largestIdx];
    const smallestUnit = units[smallestIdx];
    assert.throws(RangeError, () => earlier.until(later, { largestUnit, smallestUnit }));
  }
}
