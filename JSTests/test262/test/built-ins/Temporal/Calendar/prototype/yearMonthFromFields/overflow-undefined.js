// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearmonthfromfields
description: Fallback value for overflow option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-totemporaloverflow step 1:
      1. Return ? GetOption(_normalizedOptions_, *"overflow"*, « String », « *"constrain"*, *"reject"* », *"constrain"*).
    sec-temporal-isoyearmonthfromfields step 2:
      2. Let _overflow_ be ? ToTemporalOverflow(_options_).
    sec-temporal.calendar.prototype.yearmonthfromfields step 6:
      6. Let _result_ be ? ISOYearMonthFromFields(_fields_, _options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");

const explicit = calendar.yearMonthFromFields({ year: 2000, month: 15 }, { overflow: undefined });
TemporalHelpers.assertPlainYearMonth(explicit, 2000, 12, "M12", "default overflow is constrain");
const implicit = calendar.yearMonthFromFields({ year: 2000, month: 15 }, {});
TemporalHelpers.assertPlainYearMonth(implicit, 2000, 12, "M12", "default overflow is constrain");
