// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.subtract
description: RangeError thrown when signs don't match in the duration
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarThrowEverything();
const date = new Temporal.PlainDate(2000, 5, 2, calendar);
const duration = { months: 1, days: -30 };
for (const overflow of ["constrain", "reject"]) {
  assert.throws(RangeError, () => date.subtract(duration, { overflow }));
}
