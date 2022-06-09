// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.add
description: Type conversions for overflow option
info: |
    sec-getoption step 9.a:
      a. Set _value_ to ? ToString(_value_).
    sec-temporal-totemporaloverflow step 1:
      1. Return ? GetOption(_normalizedOptions_, *"overflow"*, « String », « *"constrain"*, *"reject"* », *"constrain"*).
    sec-temporal-isoyearmonthfromfields step 2:
      2. Let _overflow_ be ? ToTemporalOverflow(_options_).
    sec-temporal.plainyearmonth.prototype.add steps 13–15:
      13. Let _addedDate_ be ? CalendarDateAdd(_calendar_, _date_, _durationToAdd_, _options_).
      14. ...
      15. Return ? YearMonthFromFields(_calendar_, _addedDateFields_, _options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const yearmonth = new Temporal.PlainYearMonth(2000, 5);
const duration = new Temporal.Duration(1, 1);

// See TemporalHelpers.checkStringOptionWrongType(); this code path has
// different expectations for observable calls

assert.throws(RangeError, () => yearmonth.add(duration, { overflow: null }), "null");
assert.throws(RangeError, () => yearmonth.add(duration, { overflow: true }), "true");
assert.throws(RangeError, () => yearmonth.add(duration, { overflow: false }), "false");
assert.throws(TypeError, () => yearmonth.add(duration, { overflow: Symbol() }), "symbol");
assert.throws(RangeError, () => yearmonth.add(duration, { overflow: 2n }), "bigint");
assert.throws(RangeError, () => yearmonth.add(duration, { overflow: {} }), "plain object");

// toString property is read once by Calendar.dateAdd() and then once again by
// calendar.yearMonthFromFields().
const expected = [
  "get overflow.toString",
  "call overflow.toString",
  "get overflow.toString",
  "call overflow.toString",
];
const actual = [];
const observer = TemporalHelpers.toPrimitiveObserver(actual, "constrain", "overflow");
const result = yearmonth.add(duration, { overflow: observer });
TemporalHelpers.assertPlainYearMonth(result, 2001, 6, "M06", "object with toString");
assert.compareArray(actual, expected, "order of operations");
