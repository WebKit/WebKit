// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.equals
description: >
    Calendar.monthDayFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const instance = new Temporal.PlainMonthDay(5, 2);
const arg = { monthCode: "M05", day: 2, calendar };
instance.equals(arg);
assert.sameValue(calendar.monthDayFromFieldsCallCount, 1, "monthDayFromFields should be called on the property bag's calendar");
