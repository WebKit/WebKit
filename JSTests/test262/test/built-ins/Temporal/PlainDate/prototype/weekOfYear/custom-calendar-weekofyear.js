// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaindate.prototype.weekofyear
description: >
  Temporal.PlainDate.prototype.weekOfYear returns undefined for all 
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
const customCalendarDate = new Temporal.PlainDate(2024, 1, 1, calendar);
assert.sameValue(customCalendarDate.weekOfYear, undefined);
