// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: >
  The appropriate error is thrown if the calendar's daysInMonth method returns a
  value that cannot be converted to a positive integer
includes: [compareArray.js]
features: [BigInt, Symbol, Temporal]
---*/

const actual = [];
class CalendarDaysInMonthWrongValue extends Temporal.Calendar {
  constructor(badValue) {
    super("iso8601");
    this._badValue = badValue;
  }
  dateFromFields(fields, options) {
    actual.push("call dateFromFields");
    return super.dateFromFields(fields, options);
  }
  daysInMonth() {
    return this._badValue;
  }
}
// daysInMonth is only called if we are subtracting a positive duration
const duration = new Temporal.Duration(1, 1);

[Infinity, -Infinity, -42].forEach((badValue) => {
  const calendar = new CalendarDaysInMonthWrongValue(badValue);
  const yearMonth = new Temporal.PlainYearMonth(2000, 5, calendar);
  assert.throws(RangeError, () => yearMonth.subtract(duration), `daysInMonth ${badValue}`);
  assert.compareArray(actual, [], "dateFromFields not called");
});

[Symbol('foo'), 31n].forEach((badValue) => {
  const calendar = new CalendarDaysInMonthWrongValue(badValue);
  const yearMonth = new Temporal.PlainYearMonth(2000, 5, calendar);
  assert.throws(TypeError, () => yearMonth.subtract(duration), `daysInMonth ${typeof badValue}`);
  assert.compareArray(actual, [], "dateFromFields not called");
});
