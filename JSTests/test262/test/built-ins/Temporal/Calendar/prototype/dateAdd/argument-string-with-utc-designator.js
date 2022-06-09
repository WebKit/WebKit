// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: RangeError thrown if a string with UTC designator is used as a PlainDate
features: [Temporal, arrow-function]
---*/

const invalidStrings = [
  "2019-10-01T09:00:00Z",
  "2019-10-01T09:00:00Z[UTC]",
];
const instance = new Temporal.Calendar("iso8601");
invalidStrings.forEach((arg) => {
  assert.throws(
    RangeError,
    () => instance.dateAdd(arg),
    "String with UTC designator should not be valid as a PlainDate"
  );
});
