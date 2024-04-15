// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.since
description: >
  A malicious calendar resulting in a year, month, or week length of zero is
  handled correctly
info: |
  RoundDuration
  10.z. If _oneYearDays_ = 0, throw a *RangeError* exception.
  ...
  11.z. If _oneMonthDays_ = 0, throw a *RangeError* exception.
  ...
  12.s. If _oneWeekDays_ = 0, throw a *RangeError* exception.
features: [Temporal]
---*/

const cal = new class extends Temporal.Calendar {
  dateAdd(date, duration, options) {
    // Called several times, last call sets oneYear/Month/WeekDays to 0
    return new Temporal.PlainDate(1970, 1, 1);
  }
}("iso8601");

const dt1 = new Temporal.PlainDateTime(1970, 1, 1, 0, 0, 0, 0, 0, 0, cal);
const dt2 = new Temporal.PlainDateTime(1971, 1, 1, 0, 0, 0, 0, 0, 1, cal);

assert.throws(RangeError, () => dt1.since(dt2, { smallestUnit: "years" }), "zero year length handled correctly");
assert.throws(RangeError, () => dt1.since(dt2, { smallestUnit: "months" }), "zero month length handled correctly");
assert.throws(RangeError, () => dt1.since(dt2, { smallestUnit: "weeks" }), "zero week length handled correctly");
