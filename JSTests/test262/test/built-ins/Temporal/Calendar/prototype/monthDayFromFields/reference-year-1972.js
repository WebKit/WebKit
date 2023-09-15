// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.monthdayfromfields
description: Use a leap year as the reference year if monthCode is given
info: |
  sec-temporal-isomonthdayfromfields:
  12. If _monthCode_ is *undefined*, then
    a. Let _result_ be ? RegulateISODate(_year_, _month_, _day_, _overflow_).
  13. Else,
    a. Let _result_ be ? RegulateISODate(_referenceISOYear_, _month_, _day_, _overflow_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const cal = new Temporal.Calendar("iso8601");

let result = cal.monthDayFromFields({ year: 2021, monthCode: "M02", day: 29} , { overflow: "constrain" });
TemporalHelpers.assertPlainMonthDay(result, "M02", 28, "year should not be ignored when monthCode is given (overflow constrain");

assert.throws(
  RangeError, 
  () => cal.monthDayFromFields({ year: 2021, monthCode: "M02", day: 29 }, {overflow: "reject"}), 
  "year should not be ignored when monthCode is given (overflow reject)"
);

result = cal.monthDayFromFields({ year: 2021, month: 2, day: 29 }, { overflow: "constrain" });
TemporalHelpers.assertPlainMonthDay(result, "M02", 28, "year should not be ignored if monthCode is not given (overflow constrain)");

assert.throws(
  RangeError,
  () => cal.monthDayFromFields({ year: 2021, month: 2, day: 29 }, { overflow: "reject" }),
  "year should not be ignored if monthCode is not given (overflow reject)"
);
