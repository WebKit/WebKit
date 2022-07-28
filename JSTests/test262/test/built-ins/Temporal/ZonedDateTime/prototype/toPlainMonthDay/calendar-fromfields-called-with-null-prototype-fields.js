// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.toplainmonthday
description: >
    Calendar.monthDayFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const instance = new Temporal.ZonedDateTime(0n, "UTC", calendar);
instance.toPlainMonthDay();
assert.sameValue(calendar.monthDayFromFieldsCallCount, 1, "monthDayFromFields should have been called on the calendar");
