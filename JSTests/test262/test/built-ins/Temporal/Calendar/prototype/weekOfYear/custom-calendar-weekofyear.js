// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.weekofyear
description: >
  Temporal.Calendar.prototype.weekOfYear returns undefined for all 
  custom calendars where weekOfYear() returns undefined.
features: [Temporal]
---*/

class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  weekOfYear() {
    return undefined;
  }
}

const calendar = new CustomCalendar();
const customCalendarDate = { month: 1, day: 1, year: 2024, calendar};
assert.sameValue(calendar.weekOfYear({...customCalendarDate}), undefined);
