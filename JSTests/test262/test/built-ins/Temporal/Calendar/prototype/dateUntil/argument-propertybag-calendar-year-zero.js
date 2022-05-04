// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
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
invalidStrings.forEach((arg) => {
  assert.throws(
    RangeError,
    () => instance.dateUntil(arg, new Temporal.PlainDate(1977, 11, 19)),
    "reject minus zero as extended year (first argument)"
  );
  assert.throws(
    RangeError,
    () => instance.dateUntil(new Temporal.PlainDate(1977, 11, 19), arg),
    "reject minus zero as extended year (second argument)"
  );
});
