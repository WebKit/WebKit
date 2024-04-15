// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindatetime.prototype.yearofweek
description: >
  Temporal.PlainDateTime.prototype.yearOfWeek returns undefined for all 
  custom calendars where yearOfWeek() returns undefined.
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  yearOfWeek() {
    return undefined;
  }
}

const calendar = new CustomCalendar();
const customCalendarDate = new Temporal.PlainDateTime(2024, 1, 1, 12, 34, 56, 987, 654, 321, calendar);
assert.sameValue(customCalendarDate.yearOfWeek, undefined);
