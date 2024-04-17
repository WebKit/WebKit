// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthcode
description: Negative zero, as an extended year, is rejected
features: [Temporal, arrow-function]
---*/

const invalidStrings = [
  "-000000-10-31",
  "-000000-10-31T17:45",
  "-000000-10-31T17:45Z",
  "-000000-10-31T17:45+01:00",
  "-000000-10-31T17:45+00:00[UTC]",
];
const instance = new Temporal.Calendar("iso8601");
invalidStrings.forEach((str) => {
  const arg = { year: 1976, month: 11, day: 18, calendar: str };
  assert.throws(
    RangeError,
    () => instance.monthCode(arg),
    "reject minus zero as extended year"
  );
});
