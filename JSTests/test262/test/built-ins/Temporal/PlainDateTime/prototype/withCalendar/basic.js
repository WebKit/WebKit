// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: Non-throwing non-edge case
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const dt = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789);
const calendar = new Temporal.Calendar("iso8601");

const result = dt.withCalendar(calendar);

TemporalHelpers.assertPlainDateTime(
  result,
  1976, 11, "M11", 18, 15, 23, 30, 123, 456, 789,
  "works"
);

assert.sameValue(result.calendar, calendar, "underlying calendar is unchanged");
