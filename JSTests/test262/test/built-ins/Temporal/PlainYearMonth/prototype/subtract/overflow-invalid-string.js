// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.prototype.subtract
description: RangeError thrown when overflow option not one of the allowed string values
info: |
    sec-getoption step 10:
      10. If _values_ is not *undefined* and _values_ does not contain an element equal to _value_, throw a *RangeError* exception.
    sec-temporal-totemporaloverflow step 1:
      1. Return ? GetOption(_normalizedOptions_, *"overflow"*, « String », « *"constrain"*, *"reject"* », *"constrain"*).
    sec-temporal-isoyearmonthfromfields step 2:
      2. Let _overflow_ be ? ToTemporalOverflow(_options_).
    sec-temporal.plainyearmonth.prototype.subtract steps 13–15:
      13. Let _addedDate_ be ? CalendarDateAdd(_calendar_, _date_, _durationToAdd_, _options_).
      14. ...
      15. Return ? YearMonthFromFields(_calendar_, _addedDateFields_, _options_).
features: [Temporal]
---*/

const yearmonth = new Temporal.PlainYearMonth(2000, 5);
const duration = new Temporal.Duration(1, 1);
assert.throws(RangeError, () => yearmonth.subtract(duration, { overflow: "other string" }));
