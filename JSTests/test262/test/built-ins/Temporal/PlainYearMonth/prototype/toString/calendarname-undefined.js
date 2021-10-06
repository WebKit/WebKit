// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainyearmonth.protoype.tostring
description: Fallback value for calendarName option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « String », « *"auto"*, *"always"*, *"never"* », *"auto"*).
    sec-temporal.plainyearmonth.protoype.tostring step 4:
      4. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const yearmonth1 = new Temporal.PlainYearMonth(2000, 5);
const yearmonth2 = new Temporal.PlainYearMonth(2000, 5, calendar);

[
  [yearmonth1, "2000-05"],
  [yearmonth2, "2000-05-01[u-ca=custom]"],
].forEach(([yearmonth, expected]) => {
  const explicit = yearmonth.toString({ calendarName: undefined });
  assert.sameValue(explicit, expected, "default calendarName option is auto");

  const implicit = yearmonth.toString({});
  assert.sameValue(implicit, expected, "default calendarName option is auto");
});
