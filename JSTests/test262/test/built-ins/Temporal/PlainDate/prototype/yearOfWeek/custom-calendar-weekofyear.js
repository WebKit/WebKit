// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.yearofweek
description: >
  Temporal.PlainDate.prototype.yearOfWeek returns undefined for all 
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
const customCalendarDate = new Temporal.PlainDate(2024, 1, 1, calendar);
assert.sameValue(customCalendarDate.yearOfWeek, undefined);
