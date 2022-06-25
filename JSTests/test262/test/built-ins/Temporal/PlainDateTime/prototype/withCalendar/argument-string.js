// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.withcalendar
description: String argument, if it names a recognizable calendar, gets cast
features: [Temporal]
includes: [temporalHelpers.js]
---*/

const calendar = {
  toString() { return "something special"; }
};
const dt = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 30, 123, 456, 789, calendar);
const result = dt.withCalendar("iso8601");

TemporalHelpers.assertPlainDateTime(
  result,
  1976, 11, "M11", 18, 15, 23, 30, 123, 456, 789,
  "'iso8601' is a recognizable calendar"
);

const resultCalendar = result.calendar;

assert.sameValue(
  resultCalendar instanceof Temporal.Calendar,
  true,
  "underlying calendar is no longer a plain object"
);

assert.sameValue(resultCalendar.toString(), "iso8601", "underlying calendar has changed");

assert.throws(
  RangeError,
  () => dt.withCalendar("this will fail"),
  "unknown calendar throws"
);
