// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.protoype.tostring
description: Type conversions for calendarName option
info: |
    sec-getoption step 9.a:
      a. Set _value_ to ? ToString(_value_).
    sec-temporal-toshowcalendaroption step 1:
      1. Return ? GetOption(_normalizedOptions_, *"calendarName"*, « String », « *"auto"*, *"always"*, *"never"* », *"auto"*).
    sec-temporal.plainmonthday.protoype.tostring step 4:
      4. Let _showCalendar_ be ? ToShowCalendarOption(_options_).
includes: [compareArray.js, temporalHelpers.js]
features: [Temporal]
---*/

const calendar = {
  toString() { return "custom"; }
};
const monthday = new Temporal.PlainMonthDay(5, 2, calendar);

TemporalHelpers.checkStringOptionWrongType("calendarName", "auto",
  (calendarName) => monthday.toString({ calendarName }),
  (result, descr) => assert.sameValue(result, "1972-05-02[u-ca=custom]", descr),
);
