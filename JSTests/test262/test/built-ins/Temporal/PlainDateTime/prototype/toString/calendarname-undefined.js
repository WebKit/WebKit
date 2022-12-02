// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.protoype.tostring
description: Fallback value for calendarName option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « *"string"* », « *"auto"*, *"always"*, *"never"*, *"critical"* », *"auto"*).
    sec-temporal.plaindatetime.protoype.tostring step 6:
      6. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
features: [Temporal]
---*/

const tests = [
  [[], "1976-11-18T15:23:00", "built-in ISO"],
  [[{ toString() { return "custom"; } }], "1976-11-18T15:23:00[u-ca=custom]", "custom"],
  [[{ toString() { return "iso8601"; } }], "1976-11-18T15:23:00", "custom with iso8601 toString"],
  [[{ toString() { return "ISO8601"; } }], "1976-11-18T15:23:00[u-ca=ISO8601]", "custom with caps toString"],
  [[{ toString() { return "\u0131so8601"; } }], "1976-11-18T15:23:00[u-ca=\u0131so8601]", "custom with dotless i toString"],
];

for (const [args, expected, description] of tests) {
  const datetime = new Temporal.PlainDateTime(1976, 11, 18, 15, 23, 0, 0, 0, 0, ...args);
  const result = datetime.toString({ calendarName: undefined });
  assert.sameValue(result, expected, `default calendarName option is auto with ${description} calendar`);
  // See options-object.js for {} and options-undefined.js for absent options arg
}
