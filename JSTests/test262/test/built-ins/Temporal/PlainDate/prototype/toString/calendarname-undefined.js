// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.protoype.tostring
description: Fallback value for calendarName option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « String », « *"auto"*, *"always"*, *"never"* », *"auto"*).
    sec-temporal.plaindate.protoype.tostring step 4:
      4. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const date1 = new Temporal.PlainDate(2000, 5, 2);
const date2 = new Temporal.PlainDate(2000, 5, 2, calendar);

[
  [date1, "2000-05-02"],
  [date2, "2000-05-02[u-ca=custom]"],
].forEach(([date, expected]) => {
  const explicit = date.toString({ calendarName: undefined });
  assert.sameValue(explicit, expected, "default calendarName option is auto");

  const implicit = date.toString({});
  assert.sameValue(implicit, expected, "default calendarName option is auto");
});
