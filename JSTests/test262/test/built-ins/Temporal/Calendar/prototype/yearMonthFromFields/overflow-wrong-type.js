// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Type conversions for overflow option
info: |
    sec-getoption step 9.a:
      a. Set _value_ to ? ToString(_value_).
    sec-temporal-totemporaloverflow step 1:
      1. Return ? GetOption(_normalizedOptions_, *"overflow"*, « String », « *"constrain"*, *"reject"* », *"constrain"*).
    sec-temporal-isoyearmonthfromfields step 2:
      2. Let _overflow_ be ? ToTemporalOverflow(_options_).
    sec-temporal.calendar.prototype.yearmonthfromfields step 6:
      6. Let _result_ be ? ISOYearMonthFromFields(_fields_, _options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
TemporalHelpers.checkStringOptionWrongType("overflow", "constrain",
  (overflow) => calendar.yearMonthFromFields({ year: 2000, month: 5 }, { overflow }),
  (result, descr) => TemporalHelpers.assertPlainYearMonth(result, 2000, 5, "M05", descr),
);
