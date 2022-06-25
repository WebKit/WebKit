// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateadd
description: Fallback value for overflow option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-totemporaloverflow step 1:
      1. Return ? GetOption(_normalizedOptions_, *"overflow"*, « String », « *"constrain"*, *"reject"* », *"constrain"*).
    sec-temporal.calendar.prototype.dateadd step 7:
      7. Let _overflow_ be ? ToTemporalOverflow(_options_).
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = new Temporal.Calendar("iso8601");
const date = new Temporal.PlainDate(2000, 5, 31, calendar);
const duration = new Temporal.Duration(3, 1);

const explicit = calendar.dateAdd(date, duration, { overflow: undefined });
TemporalHelpers.assertPlainDate(explicit, 2003, 6, "M06", 30, "default overflow is constrain");
const implicit = calendar.dateAdd(date, duration, {});
TemporalHelpers.assertPlainDate(implicit, 2003, 6, "M06", 30, "default overflow is constrain");
