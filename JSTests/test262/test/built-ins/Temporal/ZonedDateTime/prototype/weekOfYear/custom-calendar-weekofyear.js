// Copyright (C) 2024 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.weekofyear
description: >
  Temporal.ZonedDateTime.prototype.weekOfYear returns undefined for all 
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
// Epoch Nanoseconds for new Temporal.PlainDateTime(2024, 1, 1, 12, 34, 56, 987, 654, 321, calendar);
const customCalendarDate = new Temporal.ZonedDateTime(1_704_112_496_987_654_321n, "UTC", calendar);
assert.sameValue(customCalendarDate.weekOfYear, undefined);
