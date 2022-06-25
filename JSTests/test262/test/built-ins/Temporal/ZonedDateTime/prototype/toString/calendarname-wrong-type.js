// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.protoype.tostring
description: Type conversions for calendarName option
info: |
    sec-getoption step 9.a:
      a. Set _value_ to ? ToString(_value_).
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « String », « *"auto"*, *"always"*, *"never"* », *"auto"*).
    sec-temporal.zoneddatetime.protoype.tostring step 6:
      6. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const datetime = new Temporal.ZonedDateTime(1_000_000_000_987_654_321n, "UTC", calendar);

TemporalHelpers.checkStringOptionWrongType("calendarName", "auto",
  (calendarName) => datetime.toString({ calendarName }),
  (result, descr) => assert.sameValue(result, "2001-09-09T01:46:40.987654321+00:00[UTC][u-ca=custom]", descr),
);
