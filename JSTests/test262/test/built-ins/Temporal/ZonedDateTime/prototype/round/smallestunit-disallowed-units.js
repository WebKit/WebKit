// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.round
description: Specifically disallowed units for the smallestUnit option
features: [Temporal, arrow-function]
---*/

const instance = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC");
const invalidUnits = [
  "era",
  "eras",
  "year",
  "month",
  "week",
  "years",
  "months",
  "weeks",
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
