// Copyright (C) 2023 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: RangeError thrown when subtracting positive duration and end of month is out of range
features: [Temporal]
info: |
  AddDurationToOrSubtractDurationFromPlainYearMonth:
  12. If _sign_ &lt; 0, then
    a. Let _oneMonthDuration_ be ! CreateTemporalDuration(0, 1, 0, 0, 0, 0, 0, 0, 0, 0).
    b. Let _nextMonth_ be ? CalendarDateAdd(_calendar_, _intermediateDate_, _oneMonthDuration_, *undefined*, _dateAdd_).
    c. Let _endOfMonthISO_ be ! AddISODate(_nextMonth_.[[ISOYear]], _nextMonth_.[[ISOMonth]], _nextMonth_.[[ISODay]], 0, 0, 0, -1, *"constrain"*).
    d. Let _endOfMonth_ be ? CreateTemporalDate(_endOfMonthISO_.[[Year]], _endOfMonthISO_.[[Month]], _endOfMonthISO_.[[Day]], _calendar_).
---*/

// Based on a test case by André Bargull <andre.bargull@gmail.com>

const duration = new Temporal.Duration(0, 0, 0, 1);

// Calendar addition result is out of range
assert.throws(RangeError, () => new Temporal.PlainYearMonth(275760, 9).subtract(duration), "Addition of 1 month to receiver out of range");

// Calendar addition succeeds, but subtracting 1 day gives out of range result
const cal = new class extends Temporal.Calendar {
  dateAdd() {
    return new Temporal.PlainDate(-271821, 4, 19);
  }
}("iso8601");
assert.throws(RangeError, () => new Temporal.PlainYearMonth(2000, 1, cal).subtract(duration), "Subtraction of 1 day from next month out of range");
