// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.yearofweek
description: >
  Temporal.Calendar.prototype.yearOfWeek returns undefined for all 
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
const customCalendarDate = { month: 1, day: 1, year: 2024, calendar};
assert.sameValue(calendar.yearOfWeek({...customCalendarDate}), undefined);
