// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.dateuntil
description: >
    Calendar.dateFromFields method is called with a null-prototype fields object
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckFieldsPrototypePollution();
const instance = new Temporal.Calendar("iso8601");
const arg1 = { year: 2000, month: 5, day: 2, calendar };
const arg2 = new Temporal.PlainDate(1977, 11, 19);

instance.dateUntil(arg1, arg2);
assert.sameValue(calendar.dateFromFieldsCallCount, 1, "dateFromFields should be called on the property bag's calendar (first argument)");

calendar.dateFromFieldsCallCount = 0;

instance.dateUntil(arg2, arg1);
assert.sameValue(calendar.dateFromFieldsCallCount, 1, "dateFromFields should be called on the property bag's calendar (second argument)");
