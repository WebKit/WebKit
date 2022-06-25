// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.tostring
description: Fallback value for calendarName option
info: |
    sec-getoption step 3:
      3. If _value_ is *undefined*, return _fallback_.
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « String », « *"auto"*, *"always"*, *"never"* », *"auto"*).
    sec-temporal.zoneddatetime.protoype.tostring step 6:
      6. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const datetime1 = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC");
const datetime2 = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);

[
  [datetime1, "2001-09-09T01:46:40.987654321+00:00[UTC]"],
  [datetime2, "2001-09-09T01:46:40.987654321+00:00[UTC][u-ca=custom]"],
].forEach(([datetime, expected]) => {
  const explicit = datetime.toString({ calendarName: undefined });
  assert.sameValue(explicit, expected, "default calendarName option is auto");

  // See options-undefined.js for {}
});

