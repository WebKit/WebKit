// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.round
description: Specifically disallowed units for the smallestUnit option
features: [Temporal, arrow-function]
---*/

const instance = new Temporal.PlainTime(12, 34, 56, 789, 999, 999);
const invalidUnits = [
  "era",
  "eras",
  "year",
  "month",
  "week",
  "years",
  "months",
  "weeks",
  "day",
  "days",
];
invalidUnits.forEach((smallestUnit) => {
  assert.throws(
    RangeError,
    () => instance.round({ smallestUnit }),
    `{ smallestUnit: "${smallestUnit}" } should not be allowed as an argument to round`
  );
  assert.throws(
    RangeError,
    () => instance.round(smallestUnit),
    `"${smallestUnit}" should not be allowed as an argument to round`
  );
});
