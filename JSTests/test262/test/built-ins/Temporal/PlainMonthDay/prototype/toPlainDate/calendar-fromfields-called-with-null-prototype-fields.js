// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.toplaindate
description: >
    Calendar.dateFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const instance = new Temporal.PlainMonthDay(5, 2, calendar);
instance.toPlainDate({ year: 2019 });
assert.sameValue(calendar.dateFromFieldsCallCount, 1, "dateFromFields should have been called on the calendar");
