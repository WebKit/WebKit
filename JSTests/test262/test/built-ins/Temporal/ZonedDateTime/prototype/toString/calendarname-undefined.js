// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.tostring
description: Fallback value for calendarName option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « *"string"* », « *"auto"*, *"always"*, *"never"*, *"critical"* », *"auto"*).
    sec-temporal.zoneddatetime.protoype.tostring step 6:
      6. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
features: [Temporal]
---*/

const tests = [
  [[], "1970-01-01T01:01:01.987654321+00:00[UTC]", "built-in ISO"],
  [[{ toString() { return "custom"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC][u-ca=custom]", "custom"],
  [[{ toString() { return "iso8601"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC]", "custom with iso8601 toString"],
  [[{ toString() { return "ISO8601"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC][u-ca=ISO8601]", "custom with caps toString"],
  [[{ toString() { return "\u0131so8601"; } }], "1970-01-01T01:01:01.987654321+00:00[UTC][u-ca=\u0131so8601]", "custom with dotless i toString"],
];

for (const [args, expected, description] of tests) {
  const datetime = new Temporal.ZonedDateTime(3661_987_654_321n, "UTC", ...args);
  const result = datetime.toString({ calendarName: undefined });
  assert.sameValue(result, expected, `default calendarName option is auto with ${description} calendar`);
  // See options-object.js for {} and options-undefined.js for absent options arg
}
