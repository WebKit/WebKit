// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.protoype.tostring
description: Fallback value for calendarName option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « String », « *"auto"*, *"always"*, *"never"* », *"auto"*).
    sec-temporal.plaindatetime.protoype.tostring step 6:
      6. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const datetime1 = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321);
const datetime2 = new Temporal.PlainDateTime(2000, 5, 2, 12, 34, 56, 987, 654, 321, calendar);

[
  [datetime1, "2000-05-02T12:34:56.987654321"],
  [datetime2, "2000-05-02T12:34:56.987654321[u-ca=custom]"],
].forEach(([datetime, expected]) => {
  const explicit = datetime.toString({ calendarName: undefined });
  assert.sameValue(explicit, expected, "default calendarName option is auto");

  // See options-undefined.js for {}
});
