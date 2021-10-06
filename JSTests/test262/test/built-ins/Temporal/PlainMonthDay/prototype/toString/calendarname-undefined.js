// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.protoype.tostring
description: Fallback value for calendarName option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « String », « *"auto"*, *"always"*, *"never"* », *"auto"*).
    sec-temporal.plainmonthday.protoype.tostring step 4:
      4. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
features: [Temporal]
---*/

const tests = [
  [[], "05-02"],
  [[{ toString() { return "custom"; } }], "1972-05-02[u-ca=custom]"],
  [[{ toString() { return "iso8601"; } }], "05-02"],
  [[{ toString() { return "ISO8601"; } }], "1972-05-02[u-ca=ISO8601]"],
  [[{ toString() { return "\u0131so8601"; } }], "1972-05-02[u-ca=\u0131so8601]"], // dotless i
];

for (const [args, expected] of tests) {
  const monthday = new Temporal.PlainMonthDay(5, 2, ...args);
  const explicit = monthday.toString({ calendarName: undefined });
  assert.sameValue(explicit, expected, "default calendarName option is auto");

  const implicit = monthday.toString({});
  assert.sameValue(implicit, expected, "default calendarName option is auto");
}
