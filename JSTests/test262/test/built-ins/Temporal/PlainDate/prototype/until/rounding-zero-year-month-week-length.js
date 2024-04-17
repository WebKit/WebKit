// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.until
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

const d1 = new Temporal.PlainDate(1970, 1, 1, cal);
const d2 = new Temporal.PlainDate(1971, 1, 1, cal);

assert.throws(RangeError, () => d1.until(d2, { smallestUnit: "years" }), "zero year length handled correctly");
assert.throws(RangeError, () => d1.until(d2, { smallestUnit: "months" }), "zero month length handled correctly");
assert.throws(RangeError, () => d1.until(d2, { smallestUnit: "weeks" }), "zero week length handled correctly");
