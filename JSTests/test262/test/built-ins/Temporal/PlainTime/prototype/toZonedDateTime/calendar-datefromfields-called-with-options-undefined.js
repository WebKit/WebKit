// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plaintime.prototype.tozoneddatetime
description: >
    Calendar.dateFromFields method is called with undefined as the options value
    when call originates internally
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarFromFieldsUndefinedOptions();
const instance = new Temporal.PlainTime(12, 34, 56, 987, 654, 321, calendar);
instance.toZonedDateTime({ plainDate: { year: 2000, month: 5, day: 3, calendar }, timeZone: new Temporal.TimeZone("UTC") });
assert.sameValue(calendar.dateFromFieldsCallCount, 1);
